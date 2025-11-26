#include "ContactDialogs.hpp"
#include <iostream>

ContactDialog::ContactDialog(Gtk::Window& parent, DB& db,
                             std::function<void()> on_saved,
                             bool editing, int contact_id)
: Gtk::Dialog(),
  m_db(db),
  m_on_saved(on_saved),
  m_editing(editing),
  m_contact_id(contact_id),
  m_ok_button("Save"),
  m_cancel_button("Cancel")
{
    set_title(m_editing ? "Edit Contact" : "Add Contact");
    set_modal(true);
    set_transient_for(parent);
    set_default_size(400, 350);

    auto* area = get_content_area();
    area->append(m_main_box);

    grid.set_row_spacing(10);
    grid.set_column_spacing(12);
    grid.set_margin_top(15);
    grid.set_margin_bottom(10);
    grid.set_margin_start(15);
    grid.set_margin_end(15);
    m_main_box.append(grid);

    // Create labels with proper alignment
    auto* lbl_first = Gtk::make_managed<Gtk::Label>("First Name:");
    auto* lbl_last  = Gtk::make_managed<Gtk::Label>("Last Name:");
    auto* lbl_email = Gtk::make_managed<Gtk::Label>("Email:");
    auto* lbl_mobile = Gtk::make_managed<Gtk::Label>("Mobile:");

    lbl_first->set_halign(Gtk::Align::END);
    lbl_last->set_halign(Gtk::Align::END);
    lbl_email->set_halign(Gtk::Align::END);
    lbl_mobile->set_halign(Gtk::Align::END);

    // Set entry widths
    entry_first.set_hexpand(true);
    entry_last.set_hexpand(true);
    entry_email.set_hexpand(true);
    entry_mobile.set_hexpand(true);
    
    // Set placeholders
    entry_first.set_placeholder_text("Enter first name");
    entry_last.set_placeholder_text("Enter last name");
    entry_email.set_placeholder_text("example@email.com");
    entry_mobile.set_placeholder_text("+44 1234 567890");

    grid.attach(*lbl_first, 0, 0);
    grid.attach(entry_first, 1, 0);
    grid.attach(*lbl_last,  0, 1);
    grid.attach(entry_last, 1, 1);
    grid.attach(*lbl_email, 0, 2);
    grid.attach(entry_email, 1, 2);
    grid.attach(*lbl_mobile, 0, 3);
    grid.attach(entry_mobile, 1, 3);

    // Validation label (initially hidden)
    m_validation_label.set_halign(Gtk::Align::START);
    m_validation_label.set_visible(false);
    m_validation_label.set_margin_top(5);
    m_validation_label.add_css_class("error");
    grid.attach(m_validation_label, 0, 4, 2, 1);

    // Button box
    m_button_box.set_spacing(10);
    m_button_box.set_margin_top(15);
    m_button_box.set_margin_bottom(12);
    m_button_box.set_margin_start(10);
    m_button_box.set_halign(Gtk::Align::END);
    m_button_box.append(m_cancel_button);
    m_button_box.append(m_ok_button);
    m_main_box.append(m_button_box);

    // Style the OK button
    m_ok_button.add_css_class("suggested-action");

    // Connect signals
    m_ok_button.signal_clicked().connect(
        sigc::mem_fun(*this, &ContactDialog::on_ok_clicked)
    );
    m_cancel_button.signal_clicked().connect(
        sigc::mem_fun(*this, &ContactDialog::on_cancel_clicked)
    );
    
    // Clear validation on input
    entry_first.signal_changed().connect([this]() { clear_validation_error(); });
    entry_last.signal_changed().connect([this]() { clear_validation_error(); });
    entry_email.signal_changed().connect([this]() { clear_validation_error(); });
    entry_mobile.signal_changed().connect([this]() { clear_validation_error(); });

    if (m_editing)
        load_contact_data();
}

void ContactDialog::on_ok_clicked()
{
    if (!validate_input()) {
        return; // Don't close dialog if validation fails
    }
    
    // Trim input values
    std::string first = trim(entry_first.get_text());
    std::string last = trim(entry_last.get_text());
    std::string email = trim(entry_email.get_text());
    std::string mobile = trim(entry_mobile.get_text());
    
    try {
        if (m_editing) {
            m_db.update_contact(m_contact_id, first, last, email, mobile);
        } else {
            m_db.insert_contact(first, last, email, mobile);
        }

        if (m_on_saved)
            m_on_saved();
            
        hide(); // This will trigger signal_hide() in MainWindow
    }
    catch (const DBException& e) {
        show_validation_error(e.what());
    }
}

void ContactDialog::on_cancel_clicked()
{
    hide(); // This will trigger signal_hide() in MainWindow
}

void ContactDialog::load_contact_data()
{
    auto contact = m_db.get_contact_by_id(m_contact_id);
    if (!contact)
        return;

    entry_first.set_text(contact->first_name);
    entry_last.set_text(contact->last_name);
    entry_email.set_text(contact->email);
    entry_mobile.set_text(contact->mobile);
}

std::string ContactDialog::trim(const std::string& str)
{
    if (str.empty())
        return str;
        
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
        
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

bool ContactDialog::validate_input()
{
    std::string first = trim(entry_first.get_text());
    std::string last = trim(entry_last.get_text());
    std::string email = trim(entry_email.get_text());
    
    // Check if at least one name is provided
    if (first.empty() && last.empty()) {
        show_validation_error("Please enter at least a first name or last name");
        return false;
    }
    
    // Validate email if provided
    if (!email.empty() && !DB::is_valid_email(email)) {
        show_validation_error("Please enter a valid email address");
        return false;
    }
    
    clear_validation_error();
    return true;
}

void ContactDialog::show_validation_error(const std::string& message)
{
    m_validation_label.set_text("⚠ " + message);
    m_validation_label.set_visible(true);
}

void ContactDialog::clear_validation_error()
{
    m_validation_label.set_visible(false);
}
