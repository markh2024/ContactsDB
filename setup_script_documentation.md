# Setup Script Documentation

## Overview

The `setup.sh` script is a comprehensive, interactive installation and configuration tool for the Contacts Database Application. It automates the entire setup process from dependency installation to building the application.

---

## Table of Contents

1. [Script Structure](#script-structure)
2. [Key Features](#key-features)
3. [How It Works](#how-it-works)
4. [Functions Explained](#functions-explained)
5. [Flow Diagrams](#flow-diagrams)
6. [Usage Guide](#usage-guide)
7. [Troubleshooting](#troubleshooting)

---

## Script Structure

```
setup.sh
├── Configuration Variables
├── Utility Functions (colors, checks)
├── OS Detection
├── Installation Functions
│   ├── install_mariadb_connector()
│   ├── install_dependencies()
│   ├── setup_mariadb_service()
│   └── setup_database()
├── Build Functions
│   ├── build_app()
│   └── run_app()
├── Verification Functions
│   └── check_dependencies()
├── User Interface
│   └── show_menu()
└── Main Entry Point
    └── main()
```

---

## Key Features

### 1. **Multi-Distribution Support**
- **Debian/Ubuntu**: Uses `apt` package manager
- **Fedora/RHEL/CentOS**: Uses `dnf` package manager
- **Arch/Manjaro**: Uses `pacman` package manager

### 2. **Interactive Menu System**
- User-friendly interface
- Step-by-step or full automated setup
- Dependency verification

### 3. **Color-Coded Output**
```bash
GREEN  ✓ Success messages
RED    ✗ Error messages
BLUE   ℹ Information messages
YELLOW ⚠ Warning messages
```

### 4. **Error Handling**
```bash
set -e  # Exit on error
```
- Script stops immediately if any command fails
- Prevents cascading failures
- Safe to run multiple times

### 5. **Idempotent Operations**
- Can be run multiple times safely
- Checks if components already installed
- Skips unnecessary steps

---

## How It Works

### Script Execution Flow

```
┌─────────────────────┐
│   User runs script  │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  main() function    │
│  - Check bash       │
│  - Check not root   │
│  - Detect OS        │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   show_menu()       │
│  Display options    │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  User selects       │
│  option 1-8 or 0    │
└──────────┬──────────┘
           │
           ▼
     ┌─────┴─────┐
     │  Execute  │
     │  selected │
     │  function │
     └─────┬─────┘
           │
           ▼
┌─────────────────────┐
│  Return to menu     │
└─────────────────────┘
```

---

## Functions Explained

### 1. **Configuration & Utilities**

#### Color Variables
```bash
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color
```

**What are these?**
- ANSI escape codes for terminal colors
- `\033[` starts an escape sequence
- Numbers specify the color
- `m` ends the escape sequence
- `NC` resets to default color

**Usage:**
```bash
echo -e "${GREEN}Success${NC}"
# Output: Success (in green)
```

#### Print Functions
```bash
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}
```

**How it works:**
- `echo -e` enables interpretation of backslash escapes
- `$1` is the first argument passed to function
- Example: `print_success "Done"` → `✓ Done` (in green)

#### Command Existence Check
```bash
command_exists() {
    command -v "$1" >/dev/null 2>&1
}
```

**Breakdown:**
- `command -v` checks if command exists
- `>/dev/null` redirects standard output to nowhere (hides it)
- `2>&1` redirects errors to same place
- Returns 0 (true) if found, 1 (false) if not

**Usage:**
```bash
if command_exists mysql; then
    echo "MySQL is installed"
fi
```

---

### 2. **OS Detection**

```bash
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
        OS_CODENAME=${VERSION_CODENAME:-unknown}
    else
        print_error "Cannot detect OS"
        exit 1
    fi
}
```

**How it works:**

1. **Check for `/etc/os-release`**
   - Standard file on all modern Linux distributions
   - Contains OS identification data

2. **Source the file**: `. /etc/os-release`
   - `.` is shorthand for `source`
   - Loads variables from file into current shell
   - Example contents:
     ```
     ID=ubuntu
     VERSION_ID="22.04"
     VERSION_CODENAME=jammy
     ```

3. **Extract variables**
   - `$ID` → Distribution name (ubuntu, debian, fedora)
   - `$VERSION_ID` → Version number
   - `$VERSION_CODENAME` → Code name (jammy, bookworm)

4. **Parameter Expansion**: `${VERSION_CODENAME:-unknown}`
   - If `VERSION_CODENAME` is empty, use "unknown"
   - The `:-` syntax provides a default value

---

### 3. **MariaDB Connector Installation**

```bash
install_mariadb_connector() {
    case $OS in
        ubuntu|debian)
            # Install prerequisites
            sudo apt install -y wget libmariadb-dev libmariadb-dev-compat
            
            # Download .deb package
            wget -O "$DEB_FILE" "$DEB_URL"
            
            # Install package
            sudo dpkg -i "$DEB_FILE" || sudo apt -f install -y
            
            # Clean up
            rm -f "$DEB_FILE"
            ;;
    esac
}
```

**Step-by-step:**

1. **Install prerequisites**
   ```bash
   sudo apt install -y wget libmariadb-dev libmariadb-dev-compat
   ```
   - `sudo` runs command with root privileges
   - `-y` automatically answers "yes" to prompts
   - `wget` download tool
   - `libmariadb-dev` development libraries

2. **Download connector**
   ```bash
   wget -O "$DEB_FILE" "$DEB_URL"
   ```
   - `-O` specifies output filename
   - `$DEB_FILE` = `/tmp/mariadb-connector-cpp.deb`
   - Downloads from official MariaDB repository

3. **Install package**
   ```bash
   sudo dpkg -i "$DEB_FILE" || sudo apt -f install -y
   ```
   - `dpkg -i` installs Debian package
   - `||` means "or" (if first fails, run second)
   - `apt -f install` fixes broken dependencies
   - This handles both success and dependency issues

4. **Clean up**
   ```bash
   rm -f "$DEB_FILE"
   ```
   - `-f` force (no error if file doesn't exist)
   - Removes downloaded file to save space

**Why check if already installed?**
```bash
if [ -f "/usr/include/mariadb/conncpp.hpp" ]; then
    print_warning "Already installed"
    read -p "Reinstall? (y/N): " -n 1 -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        return 0
    fi
fi
```
- `-f` checks if file exists
- `read -n 1` reads single character
- `=~` regex match operator
- `^[Yy]$` matches 'Y' or 'y' exactly
- `!` negates the condition
- Returns early if user doesn't want to reinstall

---

### 4. **Dependency Installation**

```bash
install_dependencies() {
    case $OS in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y \
                build-essential \
                cmake \
                pkg-config \
                libgtkmm-4.0-dev \
                mariadb-server \
                mariadb-client
            
            install_mariadb_connector
            ;;
    esac
}
```

**Package explanations:**

| Package | Purpose |
|---------|---------|
| `build-essential` | C++ compiler (g++), make, and essential build tools |
| `cmake` | Build system generator |
| `pkg-config` | Library metadata tool |
| `libgtkmm-4.0-dev` | GTK4 C++ bindings (development files) |
| `mariadb-server` | Database server |
| `mariadb-client` | Command-line client |

**Line continuation:**
```bash
sudo apt install -y \
    package1 \
    package2
```
- `\` at end of line continues to next line
- Makes long commands readable
- All packages installed in single transaction

---

### 5. **MariaDB Service Setup**

```bash
setup_mariadb_service() {
    # Start service
    sudo systemctl start mariadb
    
    # Enable on boot
    sudo systemctl enable mariadb
    
    # Check if running
    if sudo systemctl is-active --quiet mariadb; then
        print_success "MariaDB is running"
    fi
}
```

**systemctl commands:**

| Command | Purpose |
|---------|---------|
| `start mariadb` | Start the service now |
| `enable mariadb` | Start on boot automatically |
| `is-active --quiet` | Check if running (silent) |

**Return codes:**
- `is-active` returns 0 if running, non-zero if not
- `--quiet` suppresses output
- Used in `if` statement for conditional logic

**Error handling:**
```bash
if sudo systemctl start mariadb 2>/dev/null; then
    print_success "Started"
else
    print_error "Failed to start"
    return 1
fi
```
- `2>/dev/null` hides error messages
- Checks return code for success/failure
- `return 1` exits function with error code

---

### 6. **Database Initialization**

```bash
setup_database() {
    # Check if schema.sql exists
    if [ ! -f "schema.sql" ]; then
        # Create default schema
        cat > schema.sql << 'EOF'
CREATE DATABASE IF NOT EXISTS Contacts;
USE Contacts;
-- ... SQL commands ...
EOF
    fi
    
    # Initialize database
    if mysql -u root < schema.sql 2>/dev/null; then
        print_success "Initialized (no password)"
    else
        mysql -u root -p < schema.sql
    fi
}
```

**Here-document (heredoc):**
```bash
cat > schema.sql << 'EOF'
content here
EOF
```
- `<<` starts heredoc
- `'EOF'` delimiter (quoted prevents variable expansion)
- Everything until `EOF` written to file
- `cat >` redirects to file

**Conditional file check:**
```bash
if [ ! -f "schema.sql" ]; then
```
- `!` negates condition
- `-f` checks if regular file exists
- If file doesn't exist, create it

**Input redirection:**
```bash
mysql -u root < schema.sql
```
- `<` redirects file content to command's stdin
- mysql reads and executes SQL commands from file

**Password handling:**
```bash
if mysql -u root < schema.sql 2>/dev/null; then
    # Success without password
else
    # Try with password
    mysql -u root -p < schema.sql
fi
```
- First try without password (common on fresh install)
- If fails, try with `-p` flag (prompts for password)
- `2>/dev/null` hides error message on first attempt

**Database verification:**
```bash
if mysql -u root -e "USE Contacts; SHOW TABLES;" 2>/dev/null | grep -q "contacts"; then
    print_success "Verified"
fi
```
- `-e` executes SQL command directly
- `|` pipes output to grep
- `grep -q` quiet mode (just checks if match exists)
- Returns true if "contacts" table found

---

### 7. **Building the Application**

```bash
build_app() {
    # Clean old build
    if [ -d "build" ]; then
        rm -rf build
    fi
    
    # Create and enter build directory
    mkdir build
    cd build
    
    # Run CMake
    cmake .. 2>&1 | tee cmake_output.log
    
    # Build
    make 2>&1 | tee make_output.log
    
    cd ..
}
```

**Directory operations:**
```bash
if [ -d "build" ]; then
    rm -rf build
fi
```
- `-d` checks if directory exists
- `rm -rf` removes recursively and forcefully
- Ensures clean build environment

**Output logging:**
```bash
cmake .. 2>&1 | tee cmake_output.log
```
- `2>&1` redirects stderr to stdout
- `|` pipes output to tee
- `tee` displays output AND writes to file
- Result: See output in terminal AND save to log

**Build directory pattern:**
```bash
mkdir build
cd build
cmake ..
```
- CMake best practice: build in separate directory
- `..` refers to parent directory (where CMakeLists.txt is)
- Keeps source directory clean
- Easy to delete and rebuild

**Conditional success:**
```bash
if cmake ..; then
    print_success "CMake successful"
else
    print_error "CMake failed"
    return 1
fi
```
- `if command; then` checks command's exit code
- 0 = success, non-zero = failure
- `return 1` exits function with error code

---

### 8. **Dependency Checking**

```bash
check_dependencies() {
    local all_ok=true
    
    # Check each dependency
    if command_exists g++; then
        print_success "g++ found"
    else
        print_error "g++ not found"
        all_ok=false
    fi
    
    # ... check others ...
    
    if [ "$all_ok" = true ]; then
        print_success "All satisfied!"
    fi
}
```

**Local variables:**
```bash
local all_ok=true
```
- `local` makes variable function-scoped
- Won't pollute global namespace
- Destroyed when function exits

**String comparison:**
```bash
if [ "$all_ok" = true ]; then
```
- `=` tests string equality
- Quotes prevent word splitting
- Single `=` for strings, `-eq` for numbers

**Package version check:**
```bash
if pkg-config --exists gtkmm-4.0; then
    version=$(pkg-config --modversion gtkmm-4.0)
    print_success "GTKmm-4.0: $version"
fi
```
- `--exists` checks if package known to pkg-config
- `--modversion` returns version string
- `$()` command substitution (captures output)

**Service status check:**
```bash
if sudo systemctl is-active --quiet mariadb 2>/dev/null; then
    print_success "MariaDB running"
fi
```
- `is-active` checks if service currently running
- `--quiet` suppresses output
- `2>/dev/null` hides errors if systemctl not available

---

### 9. **Menu System**

```bash
show_menu() {
    print_header "Menu"
    echo "1) Option 1"
    echo "0) Exit"
    read -p "Enter choice: " choice
    
    case $choice in
        1)
            function1
            show_menu  # Recursive call
            ;;
        0)
            exit 0
            ;;
        *)
            print_error "Invalid"
            show_menu
            ;;
    esac
}
```

**Read user input:**
```bash
read -p "Enter choice: " choice
```
- `-p` displays prompt
- Reads input into variable `choice`
- Waits for user to press Enter

**Case statement:**
```bash
case $choice in
    1)
        commands
        ;;
    0)
        exit 0
        ;;
    *)
        default case
        ;;
esac
```
- Matches `$choice` against patterns
- `1)` matches literal "1"
- `0)` matches literal "0"
- `*)` matches anything else (default)
- `;;` ends case block
- `esac` ends case statement

**Recursive menu:**
```bash
1)
    install_dependencies
    read -p "Press Enter to continue..."
    show_menu  # Call itself again
    ;;
```
- Function calls itself
- Creates loop back to menu
- Pauses with "Press Enter" for user to see results

**Full setup chain:**
```bash
5)
    install_dependencies && \
    setup_mariadb_service && \
    setup_database && \
    build_app
    ;;
```
- `&&` chains commands (AND operator)
- Next command only runs if previous succeeded
- If any fails, chain stops
- `\` continues line

---

### 10. **Main Function**

```bash
main() {
    clear
    
    # Check bash
    if [ -z "$BASH_VERSION" ]; then
        print_error "Must run with bash"
        exit 1
    fi
    
    # Check not root
    if [ "$EUID" -eq 0 ]; then
        print_error "Don't run as root"
        exit 1
    fi
    
    detect_os
    show_menu
}

main  # Execute main function
```

**Clear screen:**
```bash
clear
```
- Clears terminal display
- Gives clean slate for output

**Bash check:**
```bash
if [ -z "$BASH_VERSION" ]; then
```
- `-z` checks if string is empty
- `$BASH_VERSION` set only in bash shell
- Ensures script uses bash features

**Root check:**
```bash
if [ "$EUID" -eq 0 ]; then
```
- `$EUID` = Effective User ID
- 0 = root user
- `-eq` numeric equality
- Prevents running entire script as root (security)

**Why not run as root?**
- Dangerous: all commands have full system access
- Files created owned by root
- Script uses `sudo` only when needed
- More secure: principle of least privilege

---

## Flow Diagrams

### Full Setup Flow (Option 5)

```
Start
  │
  ▼
Install Dependencies
  ├─ Detect OS
  ├─ Install system packages
  │  ├─ Ubuntu: apt install
  │  ├─ Fedora: dnf install
  │  └─ Arch: pacman -S
  └─ Install MariaDB Connector
     ├─ Download .deb (Ubuntu/Debian)
     ├─ Install package
     └─ Clean up temp files
  │
  ▼
Setup MariaDB Service
  ├─ Start service: systemctl start
  ├─ Enable on boot: systemctl enable
  ├─ Verify running
  └─ Optional: mysql_secure_installation
  │
  ▼
Initialize Database
  ├─ Check/Create schema.sql
  ├─ Execute SQL
  │  ├─ Try without password
  │  └─ Fall back to password prompt
  └─ Verify database created
  │
  ▼
Build Application
  ├─ Clean old build
  ├─ Create build directory
  ├─ Run CMake
  ├─ Run make
  └─ Verify executable created
  │
  ▼
Done
```

### Error Handling Flow

```
Command Execution
  │
  ▼
Success? ──Yes──> Continue
  │
  No
  │
  ▼
Catch Error
  │
  ├─ Print error message
  ├─ Log to file (if applicable)
  ├─ Return error code
  └─ Stop execution (set -e)
```

---

## Usage Guide

### First Time Setup

1. **Make script executable:**
   ```bash
   chmod +x setup.sh
   ```

2. **Run the script:**
   ```bash
   ./setup.sh
   ```

3. **Choose option 5** (Full setup)
   - Installs all dependencies
   - Configures MariaDB
   - Creates database
   - Builds application

4. **Wait for prompts:**
   - May ask for sudo password
   - May ask for MySQL root password
   - May ask to run mysql_secure_installation

5. **Run application:**
   ```bash
   ./build/ContactsApp
   ```

### Individual Steps

**Only install dependencies:**
```bash
./setup.sh
# Choose option 1
```

**Only build:**
```bash
./setup.sh
# Choose option 4
```

**Check what's installed:**
```bash
./setup.sh
# Choose option 7
```

---

## Troubleshooting

### Script Won't Run

**Problem:** `Permission denied`
```bash
bash: ./setup.sh: Permission denied
```

**Solution:**
```bash
chmod +x setup.sh
```

---

### Wrong Shell

**Problem:** `This script must be run with bash`

**Solution:** Run explicitly with bash:
```bash
bash setup.sh
```

---

### MariaDB Won't Start

**Problem:** Service fails to start

**Check status:**
```bash
sudo systemctl status mariadb
```

**View logs:**
```bash
sudo journalctl -u mariadb -n 50
```

**Common fixes:**
```bash
# Reset MariaDB
sudo mysql_install_db --user=mysql
sudo systemctl restart mariadb
```

---

### CMake Configuration Fails

**Problem:** Can't find GTKmm or MariaDB

**Solution:** Install development packages:
```bash
# Ubuntu/Debian
sudo apt install libgtkmm-4.0-dev libmariadb-dev

# Check if installed
pkg-config --modversion gtkmm-4.0
```

---

### Build Fails

**Problem:** Compilation errors

**Check logs:**
```bash
cat build/make_output.log
```

**Common issues:**
- Missing headers: Install -dev packages
- Wrong C++ version: Check CMakeLists.txt requires C++17
- Connector not found: Reinstall MariaDB Connector

---

### Database Connection Fails

**Problem:** App can't connect to database

**Test manually:**
```bash
mysql -u root -p
USE Contacts;
SHOW TABLES;
```

**Reset password:**
```bash
sudo mysql
ALTER USER 'root'@'localhost' IDENTIFIED BY 'newpassword';
FLUSH PRIVILEGES;
```

---

## Advanced Usage

### Silent Installation

For automated deployments:

```bash
# Non-interactive full setup
echo "5" | ./setup.sh

# Or modify script to skip prompts
```

### Custom Paths

Modify script variables:

```bash
# Change build directory
BUILD_DIR="my_build"

# Change database name
DB_NAME="MyContacts"
```

### Different MariaDB Version

Edit connector URL:

```bash
DEB_URL="https://dlm.mariadb.com/[version]/connector-cpp-[version].deb"
```

---

## Security Considerations

### 1. **Passwords Not Saved**
The script NEVER saves passwords to files.

### 2. **Sudo Only When Needed**
Script uses `sudo` only for system operations.

### 3. **File Permissions**
Database config files created with restricted permissions (600).

### 4. **Temp File Cleanup**
Downloaded files removed after installation.

### 5. **Input Validation**
Menu choices validated before execution.

---

## Summary

The `setup.sh` script provides:

-  **Automated setup** for multiple Linux distributions
-  **Interactive menu** for step-by-step control
-  **Error handling** with clear messages
-  **Dependency verification** before building
-  **Color-coded output** for easy reading
-  **Logging** for troubleshooting
-  **Idempotent** operations (safe to re-run)
-  **Clean separation** of concerns

It handles the complete workflow from a fresh Linux install to a running application, making deployment simple and reliable.
