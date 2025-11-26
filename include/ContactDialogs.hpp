#pragma once
#include <gtkmm.h>
#include <functional>
#include "DB.hpp"

class ContactDialog : public Gtk::Dialog
{
public:
    ContactDialog(Gtk::Window& parent,
                  DB& db,
                  std::function<void()> on_saved = nullptr,
                  bool editing = false,
                  int contact_id = 0);

private:
    DB& m_db;
    std::function<void()> m_on_saved;
    bool m_editing;
    int m_contact_id;

    Gtk::Box m_main_box{Gtk::Orientation::VERTICAL};
    Gtk::Grid grid;
    Gtk::Box m_button_box{Gtk::Orientation::HORIZONTAL};

    Gtk::Entry entry_first;
    Gtk::Entry entry_last;
    Gtk::Entry entry_email;
    Gtk::Entry entry_mobile;
    
    Gtk::Label m_validation_label;

    Gtk::Button m_ok_button;
    Gtk::Button m_cancel_button;

    void on_ok_clicked();
    void on_cancel_clicked();
    void load_contact_data();
    bool validate_input();
    void show_validation_error(const std::string& message);
    void clear_validation_error();
    std::string trim(const std::string& str);
};
