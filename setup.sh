#!/usr/bin/env bash
# Unified Setup Script for Contacts Application (Improved)
# - safer failure handling
# - improved OS/service detection
# - better package install flow
# - optional secure password storage in ~/.my.cnf
# - improved user prompts / helpful messages

set -euo pipefail
IFS=$'\n\t'

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# MariaDB Connector/C++ package details (adjust if you want another version)
DEB_URL="https://dlm.mariadb.com/4464866/Connectors/cpp/connector-cpp-1.1.7/mariadb-connector-cpp_1.1.7-1+maria~bookworm_amd64.deb"

# Password storage options
PASSWORD_FILE="$HOME/.contacts_app_mysql_pass"
MYSQL_CNF="$HOME/.my.cnf"

# Globals
OS=""
OS_VERSION=""
OS_CODENAME=""
MYSQL_ROOT_PASSWORD=""
TMP_DEB=""

# --- UI helpers ---
print_success() { echo -e "${GREEN}✓${NC} $1"; }
print_error()   { echo -e "${RED}✗${NC} $1"; }
print_info()    { echo -e "${BLUE}ℹ${NC} $1"; }
print_warning() { echo -e "${YELLOW}⚠${NC} $1"; }
print_header()  { printf "\n=========================================\n  %s\n=========================================\n" "$1"; }

# --- cleanup on exit ---
cleanup() {
    if [[ -n "${TMP_DEB:-}" && -f "$TMP_DEB" ]]; then rm -f "$TMP_DEB"; fi
}
trap cleanup EXIT

# --- utils ---
command_exists() { command -v "$1" >/dev/null 2>&1; }

detect_os() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS=${ID,,}
        OS_VERSION=${VERSION_ID:-}
        OS_CODENAME=${VERSION_CODENAME:-unknown}
    else
        print_error "Cannot detect OS. Please install dependencies manually."
        exit 1
    fi
}

detect_mysql_service() {
    # return global MYSQL_SERVICE var (mariadb or mysql)
    local s
    for s in mariadb mysql; do
        if systemctl list-unit-files | grep -q "^${s}.service"; then
            echo "$s"
            return 0
        fi
    done
    # fallback
    echo "mariadb"
}

save_password_file() {
    echo "$MYSQL_ROOT_PASSWORD" > "$PASSWORD_FILE"
    chmod 600 "$PASSWORD_FILE"
    print_info "Password saved to $PASSWORD_FILE (600)"
}

save_to_my_cnf() {
    cat > "$MYSQL_CNF" <<EOF
[client]
user=root
password=$MYSQL_ROOT_PASSWORD
EOF
    chmod 600 "$MYSQL_CNF"
    print_info "Stored credentials in $MYSQL_CNF (600). mysql client will use this file."
}

load_password() {
    if [[ -f "$PASSWORD_FILE" ]]; then
        MYSQL_ROOT_PASSWORD="$(<"$PASSWORD_FILE")"
        return 0
    fi
    if [[ -f "$MYSQL_CNF" ]]; then
        # naive parse (expect standard format)
        MYSQL_ROOT_PASSWORD="$(awk -F= '/^password/ {print $2; exit}' "$MYSQL_CNF" | tr -d '[:space:]')"
        [[ -n "$MYSQL_ROOT_PASSWORD" ]] && return 0 || return 1
    fi
    return 1
}

# Executes mysql client with password (or using ~/.my.cnf if present)
mysql_exec() {
    # usage: mysql_exec [-e "SQL"] [database]
    if [[ -z "${MYSQL_ROOT_PASSWORD:-}" ]]; then
        if ! load_password; then
            if ! get_mysql_password; then
                return 1
            fi
        fi
    fi

    # prefer ~/.my.cnf (mysql picks it up automatically)
    if [[ -f "$MYSQL_CNF" ]]; then
        mysql "$@"
    else
        MYSQL_PWD="$MYSQL_ROOT_PASSWORD" mysql -u root "$@"
    fi
}

get_mysql_password() {
    # If root access without password works (common on fresh installs), ask user if they want to set a password
    if mysql -u root -e "SELECT 1;" >/dev/null 2>&1; then
        print_info "Root account accessible without password (local socket)."
        read -p "Would you like to set a root password now? (recommended) [Y/n]: " -r
        if [[ "$REPLY" =~ ^[Nn]$ ]]; then
            MYSQL_ROOT_PASSWORD=""
            return 0
        fi
        # prompt for new password
        while true; do
            read -s -p "Enter new MySQL root password: " NEW_PASSWORD
            echo
            read -s -p "Confirm password: " CONFIRM_PASSWORD
            echo
            if [[ -z "$NEW_PASSWORD" ]]; then
                print_error "Password cannot be empty"
            elif [[ "$NEW_PASSWORD" != "$CONFIRM_PASSWORD" ]]; then
                print_error "Passwords do not match"
            else
                # set it
                mysql -u root <<SQL
ALTER USER 'root'@'localhost' IDENTIFIED BY '${NEW_PASSWORD}';
CREATE USER IF NOT EXISTS 'root'@'%' IDENTIFIED BY '${NEW_PASSWORD}';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
SQL
                MYSQL_ROOT_PASSWORD="$NEW_PASSWORD"
                break
            fi
        done
    else
        # Root requires password; ask user to enter it and verify
        for i in 1 2 3; do
            read -s -p "Enter MySQL root password: " MYSQL_ROOT_PASSWORD
            echo
            if MYSQL_PWD="$MYSQL_ROOT_PASSWORD" mysql -u root -e "SELECT 1;" >/dev/null 2>&1; then
                print_success "Password verified"
                break
            else
                print_error "Password incorrect"
                MYSQL_ROOT_PASSWORD=""
            fi
            if [[ $i -eq 3 ]]; then
                print_error "Failed to verify password after 3 attempts"
                return 1
            fi
        done
    fi

    # Ask where to store (optional)
    read -p "Store password for later runs? ~/.my.cnf recommended (Y/n/skip): " -r
    if [[ "$REPLY" =~ ^[Nn]$ ]]; then
        print_info "Will not store password"
    elif [[ "$REPLY" =~ ^skip$|^S$|^s$ ]]; then
        print_info "Skipping storage"
    else
        read -p "Use ~/.my.cnf (recommended) or plain file ($PASSWORD_FILE)? [cnf/file]: " -r
        if [[ "$REPLY" =~ ^file$ ]]; then
            save_password_file
        else
            save_to_my_cnf
        fi
    fi

    return 0
}

check_database_ready() {
    local svc
    svc=$(detect_mysql_service)
    if ! sudo systemctl is-active --quiet "$svc" >/dev/null 2>&1; then
        return 1
    fi

    # check for Contacts database and contacts table
    if mysql_exec -e "USE Contacts; SHOW TABLES;" 2>/dev/null | grep -q "^contacts$"; then
        return 0
    fi
    return 1
}

# --- package / deps install ---
install_mariadb_connector_deb() {
    print_header "Installing MariaDB Connector/C++ (DEB)"
    TMP_DEB="$(mktemp /tmp/mariadb-connector.XXXXXX.deb)"

    # prefer wget, else curl
    if command_exists wget; then
        if ! wget -O "$TMP_DEB" "$DEB_URL"; then
            print_error "Failed to download DEB via wget"
            rm -f "$TMP_DEB"
            return 1
        fi
    elif command_exists curl; then
        if ! curl -L -o "$TMP_DEB" "$DEB_URL"; then
            print_error "Failed to download DEB via curl"
            rm -f "$TMP_DEB"
            return 1
        fi
    else
        print_error "Neither wget nor curl found. Install one and retry."
        rm -f "$TMP_DEB"
        return 1
    fi

    # try dpkg, fix deps if needed
    if sudo dpkg -i "$TMP_DEB"; then
        print_success "DEB installed"
    else
        print_warning "dpkg reported issues — attempting to fix dependencies via apt"
        sudo apt-get update
        sudo apt-get install -f -y
    fi

    # cleanup handled by trap
    return 0
}

install_mariadb_connector() {
    print_header "Installing MariaDB Connector/C++"
    case "$OS" in
        ubuntu|debian)
            print_info "Detected Debian-based system"
            sudo apt-get update
            sudo apt-get install -y wget libmariadb-dev libmariadb-dev-compat || true
            # if header already present, ask to reinstall
            if [[ -f "/usr/include/mariadb/conncpp.hpp" ]] || [[ -f "/usr/local/include/mariadb/conncpp.hpp" ]]; then
                print_warning "MariaDB Connector/C++ headers already present"
                read -p "Reinstall connector package? (y/N): " -r
                if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
                    return 0
                fi
            fi
            install_mariadb_connector_deb
            ;;
        fedora|rhel|centos)
            print_info "Detected RHEL-family system"
            sudo dnf install -y mariadb-connector-c++-devel
            ;;
        arch|manjaro)
            print_info "Detected Arch-based system"
            sudo pacman -S --noconfirm mariadb-libs
            ;;
        *)
            print_error "Unsupported OS for automatic connector installation"
            return 1
            ;;
    esac
}

install_dependencies() {
    print_header "Installing System Dependencies"
    print_info "OS: $OS ${OS_VERSION:-unknown}"
    case "$OS" in
        ubuntu|debian)
            sudo apt-get update
            sudo apt-get install -y \
                build-essential cmake pkg-config libgtkmm-4.0-dev \
                mariadb-server mariadb-client
            print_success "System packages installed"
            install_mariadb_connector
            ;;
        fedora|rhel|centos)
            sudo dnf install -y gcc-c++ cmake pkg-config gtkmm4.0-devel mariadb-server mariadb
            ;;
        arch|manjaro)
            sudo pacman -S --noconfirm base-devel cmake pkg-config gtkmm-4.0 mariadb
            ;;
        *)
            print_error "Unsupported OS for automatic install"
            exit 1
            ;;
    esac
}

setup_mariadb_service() {
    print_header "Setting up MariaDB service"
    local svc
    svc=$(detect_mysql_service)
    print_info "Using service name: $svc"

    if ! sudo systemctl start "$svc"; then
        print_error "Failed to start $svc. Try: sudo systemctl start $svc"
        return 1
    fi
    sudo systemctl enable "$svc" || print_warning "Could not enable $svc on boot"

    if sudo systemctl is-active --quiet "$svc"; then
        print_success "$svc is running"
    else
        print_error "$svc is not running"
        return 1
    fi

    # Configure root password or ensure remote root exists
    if mysql -u root -e "SELECT 1;" >/dev/null 2>&1; then
        print_info "Root accessible locally without password. You can set a password now."
        get_mysql_password || return 1
    else
        print_info "Root requires password — will ask for it"
        get_mysql_password || return 1
        # ensure remote root is configured (best-effort)
        mysql_exec <<SQL
CREATE USER IF NOT EXISTS 'root'@'%' IDENTIFIED BY '${MYSQL_ROOT_PASSWORD}';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
SQL
        print_success "Root privileges configured (best-effort)"
    fi

    read -p "Run mysql_secure_installation now? (interactive) [y/N]: " -r
    if [[ "$REPLY" =~ ^[Yy]$ ]]; then
        sudo mysql_secure_installation
    fi
}

setup_database() {
    print_header "Initializing Database"
    if ! get_mysql_password; then
        print_error "Cannot proceed without MySQL credentials"
        return 1
    fi

    if [[ ! -f "schema.sql" ]]; then
        print_info "Creating default schema.sql..."
        cat > schema.sql <<'EOF'
-- Create database if it doesn't exist
CREATE DATABASE IF NOT EXISTS Contacts;
USE Contacts;
CREATE TABLE IF NOT EXISTS contacts (
    id INT AUTO_INCREMENT PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    email VARCHAR(255),
    mobile VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_name (first_name, last_name),
    INDEX idx_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
SELECT 'Database initialized successfully!' AS Status;
EOF
        print_success "schema.sql created"
    fi

    print_info "Applying schema.sql..."
    if mysql_exec < schema.sql; then
        print_success "Database initialized"
    else
        print_error "Database initialization failed"
        return 1
    fi

    print_info "Granting privileges..."
    mysql_exec <<SQL
GRANT ALL PRIVILEGES ON Contacts.* TO 'root'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON Contacts.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
SQL

    if mysql_exec -e "USE Contacts; SHOW TABLES;" 2>/dev/null | grep -q "^contacts$"; then
        print_success "Contacts table present"
        print_info "Schema for contacts:"
        mysql_exec -e "DESCRIBE Contacts.contacts;"
    else
        print_error "Failed to verify contacts table"
        return 1
    fi
}

build_app() {
    print_header "Building Application"
    if [[ ! -f "CMakeLists.txt" ]]; then
        print_error "CMakeLists.txt not found. Run from project root."
        return 1
    fi

    rm -rf build
    mkdir -p build
    pushd build >/dev/null
    if cmake .. 2>&1 | tee cmake_output.log; then
        print_success "CMake configuration successful"
    else
        print_error "CMake failed — see build/cmake_output.log"
        popd >/dev/null
        return 1
    fi

    if make 2>&1 | tee make_output.log; then
        print_success "Build succeeded"
    else
        print_error "Build failed — see build/make_output.log"
        popd >/dev/null
        return 1
    fi
    popd >/dev/null

    if [[ -f "build/ContactsApp" ]]; then
        print_success "Executable created: build/ContactsApp"
    else
        print_warning "Executable not found in build/ContactsApp (check CMakeLists)"
    fi
}

run_app() {
    print_header "Running Application"
    if [[ ! -f "build/ContactsApp" ]]; then
        print_error "Application not built; choose build option first"
        return 1
    fi

    if ! check_database_ready; then
        print_warning "Database not ready"
        read -p "Initialize database now? [Y/n]: " -r
        if [[ ! "$REPLY" =~ ^[Nn]$ ]]; then
            setup_mariadb_service || return 1
            setup_database || return 1
        else
            print_error "Cannot run app without database"
            return 1
        fi
    fi

    ./build/ContactsApp
}

check_dependencies() {
    print_header "Checking Dependencies"
    local ok=true

    command_exists g++ || { print_error "g++ missing"; ok=false; } && print_success "g++: $(g++ --version | head -n1)"
    command_exists cmake || { print_error "cmake missing"; ok=false; } && print_success "cmake: $(cmake --version | head -n1)"
    command_exists pkg-config || { print_error "pkg-config missing"; ok=false; } || true
    if pkg-config --exists gtkmm-4.0; then
        print_success "GTKmm-4.0: $(pkg-config --modversion gtkmm-4.0)"
    else
        print_error "GTKmm-4.0 not found"
        ok=false
    fi

    if [[ -f "/usr/include/mariadb/conncpp.hpp" || -f "/usr/local/include/mariadb/conncpp.hpp" ]]; then
        print_success "MariaDB Connector/C++ headers found"
    else
        print_error "MariaDB Connector/C++ not found"
        ok=false
    fi

    command_exists mysql || { print_error "mysql client not found"; ok=false; } && print_success "mysql client: $(mysql --version | head -n1)"

    local svc
    svc=$(detect_mysql_service)
    if sudo systemctl is-active --quiet "$svc" 2>/dev/null; then
        print_success "$svc service is running"
    else
        print_error "$svc service is not running"
        ok=false
    fi

    if check_database_ready; then
        print_success "Contacts database ready"
    else
        print_warning "Contacts database not found"
    fi

    if [[ "$ok" = true ]]; then
        print_success "All dependencies appear satisfied"
    else
        print_error "Some dependencies are missing. Run option 1 to attempt installation."
    fi
}

show_menu() {
    print_header "Contacts App Setup Menu"
    cat <<'MENU'
1) Install dependencies
2) Setup MariaDB service
3) Initialize database
4) Build application
5) Full setup (1+2+3+4)
6) Run application
7) Check dependencies
8) Clean build
9) Reset stored password (remove ~/.my.cnf and password file)
0) Exit
MENU
    read -p "Enter choice [0-9]: " choice
    case "$choice" in
        1) install_dependencies ;;
        2) setup_mariadb_service ;;
        3) setup_database ;;
        4) build_app ;;
        5) install_dependencies && setup_mariadb_service && setup_database && build_app ;;
        6) run_app ;;
        7) check_dependencies ;;
        8) rm -rf build && print_success "Build cleaned" ;;
        9) rm -f "$PASSWORD_FILE" "$MYSQL_CNF" && print_success "Stored credentials removed" ;;
        0) exit 0 ;;
        *) print_error "Invalid choice" ;;
    esac
}

main() {
    if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
        # not executed under source
        detect_os
        print_header "Contacts Application Setup (Improved)"
        print_info "Detected: $OS ${OS_VERSION:-unknown} (${OS_CODENAME:-})"
        while true; do
            show_menu
            echo
            read -p "Press Enter to continue..." -r
        done
    fi
}

main
