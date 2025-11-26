#include "DBConnectionDialog.hpp"
#include "DB.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

DBConnectionDialog::DBConnectionDialog(
    std::function<void(const std::string&, unsigned int, const std::string&, const std::string&)> on_connect)
: Gtk::Window(),
  m_on_connect(on_connect)
{
    set_title("Connect to MariaDB Database");
    set_default_size(450, 350);
    set_resizable(false);

    // Setup main layout
    set_child(m_main_box);
    m_main_box.set_margin_top(15);
    m_main_box.set_margin_bottom(15);
    m_main_box.set_margin_start(15);
    m_main_box.set_margin_end(15);
    m_main_box.set_spacing(10);

    // Grid
    grid.set_row_spacing(10);
    grid.set_column_spacing(12);
    m_main_box.append(grid);

    // Labels
    auto* lbl_host = Gtk::make_managed<Gtk::Label>("Host/IP:");
    auto* lbl_port = Gtk::make_managed<Gtk::Label>("Port:");
    auto* lbl_user = Gtk::make_managed<Gtk::Label>("Username:");
    auto* lbl_pass = Gtk::make_managed<Gtk::Label>("Password:");
    auto* lbl_db   = Gtk::make_managed<Gtk::Label>("Database:");

    lbl_host->set_halign(Gtk::Align::END);
    lbl_port->set_halign(Gtk::Align::END);
    lbl_user->set_halign(Gtk::Align::END);
    lbl_pass->set_halign(Gtk::Align::END);
    lbl_db->set_halign(Gtk::Align::END);

    // Entries
    entry_host.set_hexpand(true);
    entry_port.set_hexpand(true);
    entry_user.set_hexpand(true);
    entry_password.set_hexpand(true);
    entry_database.set_hexpand(true);

    grid.attach(*lbl_host, 0, 0);
    grid.attach(entry_host, 1, 0);
    grid.attach(*lbl_port, 0, 1);
    grid.attach(entry_port, 1, 1);
    grid.attach(*lbl_user, 0, 2);
    grid.attach(entry_user, 1, 2);
    grid.attach(*lbl_pass, 0, 3);
    grid.attach(entry_password, 1, 3);
    grid.attach(*lbl_db, 0, 4);
    grid.attach(entry_database, 1, 4);

    // Default values
    entry_host.set_text("127.0.0.1");
    entry_port.set_text("3306");
    entry_user.set_text("root");
    entry_database.set_text("Contacts");
    entry_password.set_visibility(false);
    entry_password.set_input_purpose(Gtk::InputPurpose::PASSWORD);

    // Remember checkbox
    check_remember.set_label("Remember credentials");
    check_remember.set_margin_top(5);
    grid.attach(check_remember, 0, 5, 2, 1);

    // Status label
    m_status_label.set_halign(Gtk::Align::START);
    m_status_label.set_margin_top(5);
    m_status_label.set_visible(false);
    grid.attach(m_status_label, 0, 6, 2, 1);

    // Load saved credentials
    load_saved_credentials();

    // Buttons
    m_button_box.set_spacing(10);
    m_button_box.set_halign(Gtk::Align::END);
    m_button_box.append(test_button);
    m_button_box.append(cancel_button);
    m_button_box.append(ok_button);
    m_main_box.append(m_button_box);

    ok_button.add_css_class("suggested-action");

    // Connect signals
    ok_button.signal_clicked().connect([this](){ on_ok(); });
    cancel_button.signal_clicked().connect([this](){ hide(); });
    test_button.signal_clicked().connect([this](){ on_test_connection(); });

    entry_password.signal_activate().connect([this](){ on_ok(); });
}

void DBConnectionDialog::on_ok()
{
    std::string host = entry_host.get_text();
    std::string port_str = entry_port.get_text();
    std::string user = entry_user.get_text();
    std::string password = entry_password.get_text();

    if (host.empty() || port_str.empty() || user.empty()) {
        show_status("Please fill in all required fields", true);
        return;
    }

    unsigned int port;
    try {
        port = std::stoi(port_str);
        if (port == 0 || port > 65535)
            throw std::out_of_range("Port out of range");
    } catch (...) {
        show_status("Invalid port number", true);
        return;
    }

    // Save credentials
    if (check_remember.get_active())
        save_credentials();

    if (m_on_connect)
        m_on_connect(host, port, user, password);

    hide(); // Let signal_hide in main.cpp handle deletion
}

void DBConnectionDialog::on_test_connection()
{
    std::string host = entry_host.get_text();
    std::string port_str = entry_port.get_text();
    std::string user = entry_user.get_text();
    std::string password = entry_password.get_text();
    std::string database = entry_database.get_text();

    if (host.empty() || port_str.empty() || user.empty()) {
        show_status("Please fill in all required fields", true);
        return;
    }

    unsigned int port;
    try { port = std::stoi(port_str); } 
    catch (...) { show_status("Invalid port number", true); return; }

    show_status("Testing connection...", false);

    try {
        DB test_db(host, user, password, database, port);
        if (test_db.test_connection())
            show_status("✓ Connection successful!", false);
        else
            show_status("✗ Connection failed", true);
    } catch (const std::exception& e) {
        show_status("✗ Connection failed: " + std::string(e.what()), true);
    }
}

void DBConnectionDialog::load_saved_credentials()
{
    try {
        std::string config_dir = std::string(getenv("HOME")) + "/.config/contacts-app";
        std::string config_file = config_dir + "/db_config.txt";
        
        std::ifstream file(config_file);
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line)) entry_host.set_text(line);
            if (std::getline(file, line)) entry_port.set_text(line);
            if (std::getline(file, line)) entry_user.set_text(line);
            if (std::getline(file, line)) entry_database.set_text(line);
            check_remember.set_active(true);
        }
    } catch (...) {
        // Ignore errors loading saved credentials
    }
}

void DBConnectionDialog::save_credentials()
{
    try {
        std::string config_dir = std::string(getenv("HOME")) + "/.config/contacts-app";
        std::filesystem::create_directories(config_dir);
        
        std::string config_file = config_dir + "/db_config.txt";
        std::ofstream file(config_file);
        
        if (file.is_open()) {
            file << entry_host.get_text() << "\n";
            file << entry_port.get_text() << "\n";
            file << entry_user.get_text() << "\n";
            file << entry_database.get_text() << "\n";
            
            // Set restrictive permissions (owner read/write only)
            std::filesystem::permissions(config_file, 
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                std::filesystem::perm_options::replace);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save credentials: " << e.what() << "\n";
    }
}

void DBConnectionDialog::show_status(const std::string& message, bool is_error)
{
    m_status_label.set_text(message);
    m_status_label.set_visible(true);
    
    if (is_error) {
        m_status_label.add_css_class("error");
    } else {
        m_status_label.remove_css_class("error");
    }
}
