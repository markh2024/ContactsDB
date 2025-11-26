#pragma once
#include <gtkmm.h>
#include <functional>
#include <string>

class DBConnectionDialog : public Gtk::Window
{
public:
    DBConnectionDialog(std::function<void(const std::string& host,
                                          unsigned int port,
                                          const std::string& user,
                                          const std::string& password)> on_connect);

private:
    Gtk::Box m_main_box{Gtk::Orientation::VERTICAL};
    Gtk::Grid grid;
    Gtk::Entry entry_host;
    Gtk::Entry entry_port;
    Gtk::Entry entry_user;
    Gtk::Entry entry_password;
    Gtk::Entry entry_database;
    
    Gtk::CheckButton check_remember;
    Gtk::Label m_status_label;

    Gtk::Box m_button_box{Gtk::Orientation::HORIZONTAL};
    Gtk::Button ok_button{"Connect"};
    Gtk::Button cancel_button{"Cancel"};
    Gtk::Button test_button{"Test Connection"};

    std::function<void(const std::string&, unsigned int, const std::string&, const std::string&)> m_on_connect;

    void on_ok();
    void on_test_connection();
    void load_saved_credentials();
    void save_credentials();
    void show_status(const std::string& message, bool is_error);
};
