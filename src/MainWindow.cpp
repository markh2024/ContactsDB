#include "MainWindow.hpp"
#include "ContactDialogs.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

MainWindow::MainWindow(std::shared_ptr<DB> db)
: m_db(db)
{
    set_title("Contacts Database Manager");
    set_default_size(900, 600);

    // Initialize database schema
    try {
        m_db->initialize_schema();
    } catch (const DBException& e) {
        show_error("Failed to initialize database: " + std::string(e.what()));
    }

    // Setup list store and tree view
    m_ref_list_store = Gtk::ListStore::create(m_columns);
    m_tree_view.set_model(m_ref_list_store);
    setup_tree_view_columns();

    m_tree_view.set_headers_clickable(true);

    // Double-click to edit
    m_tree_view.signal_row_activated().connect(
        sigc::mem_fun(*this, &MainWindow::on_row_activated)
    );

    // Setup scrolled window
    m_scrolled_window.set_child(m_tree_view);
    m_scrolled_window.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scrolled_window.set_vexpand(true);

    // Setup search bar
    m_toolbar_box.set_spacing(10);
    m_toolbar_box.set_margin_top(10);
    m_toolbar_box.set_margin_bottom(10);
    m_toolbar_box.set_margin_start(10);
    m_toolbar_box.set_margin_end(10);

    auto* search_label = Gtk::make_managed<Gtk::Label>("Search:");
    m_toolbar_box.append(*search_label);
    m_search_entry.set_hexpand(true);
    m_search_entry.set_placeholder_text("Search contacts...");
    m_toolbar_box.append(m_search_entry);
    m_toolbar_box.append(m_clear_search_button);

    m_search_entry.signal_search_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_search_changed)
    );
    m_clear_search_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_clear_search)
    );

    // Setup button box
    m_button_box.set_spacing(10);
    m_button_box.set_margin_top(5);
    m_button_box.set_margin_bottom(5);
    m_button_box.set_margin_start(10);
    m_button_box.set_margin_end(10);

    m_button_box.append(m_add_button);
    m_button_box.append(m_edit_button);
    m_button_box.append(m_delete_button);

    auto* spacer = Gtk::make_managed<Gtk::Box>();
    spacer->set_hexpand(true);
    m_button_box.append(*spacer);

    m_button_box.append(m_import_button);
    m_button_box.append(m_export_button);

    // Button signals
    m_add_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_add_contact)
    );
    m_edit_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_edit_contact)
    );
    m_delete_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_delete_contact)
    );
    m_import_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_import_csv)
    );
    m_export_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_export_csv)
    );

    // Status bar
    m_status_box.set_margin_top(5);
    m_status_box.set_margin_bottom(5);
    m_status_box.set_margin_start(10);
    m_status_box.append(m_status_label);

    // Assemble main layout
    m_main_box.append(m_toolbar_box);
    m_main_box.append(m_scrolled_window);
    m_main_box.append(m_button_box);
    m_main_box.append(m_status_box);

    set_child(m_main_box);

    refresh_list();
    update_status();
}

void MainWindow::setup_tree_view_columns()
{
    auto* col_id = Gtk::manage(new Gtk::TreeViewColumn("ID", m_columns.col_id));
    col_id->set_visible(false);
    m_tree_view.append_column(*col_id);

    auto* col_first = Gtk::manage(new Gtk::TreeViewColumn("First Name", m_columns.col_first));
    col_first->set_resizable(true);
    col_first->set_sort_column(m_columns.col_first);
    m_tree_view.append_column(*col_first);

    auto* col_last = Gtk::manage(new Gtk::TreeViewColumn("Last Name", m_columns.col_last));
    col_last->set_resizable(true);
    col_last->set_sort_column(m_columns.col_last);
    m_tree_view.append_column(*col_last);

    auto* col_email = Gtk::manage(new Gtk::TreeViewColumn("Email", m_columns.col_email));
    col_email->set_resizable(true);
    col_email->set_sort_column(m_columns.col_email);
    m_tree_view.append_column(*col_email);

    auto* col_mobile = Gtk::manage(new Gtk::TreeViewColumn("Mobile", m_columns.col_mobile));
    col_mobile->set_resizable(true);
    col_mobile->set_sort_column(m_columns.col_mobile);
    m_tree_view.append_column(*col_mobile);
}

//-------------------- Contact Handlers --------------------

void MainWindow::on_add_contact()
{
    auto* dialog = new ContactDialog(*this, *m_db,
                                    [this](){ refresh_list(); update_status(); },
                                    false, 0);
    dialog->set_modal(true);

    // Use signal_hide to properly cleanup when dialog closes
    dialog->signal_hide().connect([dialog](){
        delete dialog;
    });

    dialog->present();
}

void MainWindow::on_edit_contact()
{
    auto id = get_selected_id();
    if (!id) {
        show_info("Please select a contact to edit");
        return;
    }

    auto* dialog = new ContactDialog(*this, *m_db,
                                    [this](){ refresh_list(); update_status(); },
                                    true, *id);
    dialog->set_modal(true);

    // Use signal_hide to properly cleanup when dialog closes
    dialog->signal_hide().connect([dialog](){
        delete dialog;
    });

    dialog->present();
}

void MainWindow::on_delete_contact()
{
    auto id = get_selected_id();
    if (!id) {
        show_info("Please select a contact to delete");
        return;
    }

    auto* confirm = new Gtk::MessageDialog(*this,
        "Are you sure you want to delete this contact?",
        false, Gtk::MessageType::QUESTION, Gtk::ButtonsType::OK_CANCEL);
    confirm->set_modal(true);

    confirm->signal_response().connect([this, confirm, id](int response_id){
        if (response_id == Gtk::ResponseType::OK) {
            try {
                m_db->delete_contact(*id);
                refresh_list();
                update_status();
                show_info("Contact deleted successfully");
            } catch (const DBException& e) {
                show_error(std::string(e.what()));
            }
        }
        confirm->close();
        delete confirm;
    });
    
    confirm->present();
}

//-------------------- Search --------------------

void MainWindow::on_search_changed()
{
    m_current_search = m_search_entry.get_text();
    refresh_list();
}

void MainWindow::on_clear_search()
{
    m_search_entry.set_text("");
    m_current_search.clear();
    refresh_list();
}

//-------------------- Row Activation --------------------

void MainWindow::on_row_activated([[maybe_unused]] const Gtk::TreeModel::Path& path,
                                  [[maybe_unused]] Gtk::TreeViewColumn* column)
{
    on_edit_contact();
}

//-------------------- Import/Export --------------------

void MainWindow::on_import_csv()
{
    auto* dialog = new Gtk::FileChooserDialog(*this, "Import Contacts from CSV", Gtk::FileChooser::Action::OPEN);
    dialog->set_modal(true);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Import", Gtk::ResponseType::ACCEPT);

    auto filter = Gtk::FileFilter::create();
    filter->set_name("CSV files");
    filter->add_pattern("*.csv");
    dialog->add_filter(filter);

    dialog->signal_response().connect([this, dialog](int response_id){
        if (response_id == Gtk::ResponseType::ACCEPT) {
            auto file = dialog->get_file();
            if (file) {
                std::ifstream infile(file->get_path());
                if (!infile.is_open()) {
                    show_error("Failed to open file");
                    dialog->close();
                    delete dialog;
                    return;
                }

                std::vector<Contact> contacts;
                std::string line;
                bool first_line = true;

                while (std::getline(infile, line)) {
                    if (first_line) { first_line = false; continue; }
                    std::stringstream ss(line);
                    std::string first, last, email, mobile;

                    std::getline(ss, first, ',');
                    std::getline(ss, last, ',');
                    std::getline(ss, email, ',');
                    std::getline(ss, mobile, ',');

                    if (!first.empty() || !last.empty())
                        contacts.push_back(Contact{0, first, last, email, mobile});
                }

                if (contacts.empty()) {
                    show_info("No valid contacts found in CSV");
                } else if (m_db->import_contacts(contacts)) {
                    refresh_list();
                    update_status();
                    show_info("Successfully imported " + std::to_string(contacts.size()) + " contacts");
                } else {
                    show_error("Failed to import contacts");
                }
            }
        }
        dialog->close();
        delete dialog;
    });

    dialog->present();
}

void MainWindow::on_export_csv()
{
    auto* dialog = new Gtk::FileChooserDialog(*this, "Export Contacts to CSV", Gtk::FileChooser::Action::SAVE);
    dialog->set_modal(true);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Export", Gtk::ResponseType::ACCEPT);

    auto filter = Gtk::FileFilter::create();
    filter->set_name("CSV files");
    filter->add_pattern("*.csv");
    dialog->add_filter(filter);

    dialog->set_current_name("contacts.csv");

    dialog->signal_response().connect([this, dialog](int response_id){
        if (response_id == Gtk::ResponseType::ACCEPT) {
            auto file = dialog->get_file();
            if (file) {
                std::ofstream outfile(file->get_path());
                if (!outfile.is_open()) {
                    show_error("Failed to create file");
                    dialog->close();
                    delete dialog;
                    return;
                }

                outfile << "First Name,Last Name,Email,Mobile\n";
                auto contacts = m_db->get_all_contacts();
                for (const auto& c : contacts) {
                    outfile << c.first_name << "," << c.last_name << "," << c.email << "," << c.mobile << "\n";
                }

                show_info("Successfully exported " + std::to_string(contacts.size()) + " contacts");
            }
        }
        dialog->close();
        delete dialog;
    });

    dialog->present();
}

//-------------------- List / Status --------------------

void MainWindow::refresh_list()
{
    m_ref_list_store->clear();

    std::vector<Contact> contacts;
    try {
        if (!m_current_search.empty())
            contacts = m_db->search_contacts(m_current_search);
        else
            contacts = m_db->get_all_contacts();

        for (const auto& c : contacts) {
            auto row = *(m_ref_list_store->append());
            row[m_columns.col_id] = c.id;
            row[m_columns.col_first] = c.first_name;
            row[m_columns.col_last] = c.last_name;
            row[m_columns.col_email] = c.email;
            row[m_columns.col_mobile] = c.mobile;
        }
    } catch (const DBException& e) {
        show_error("Failed to load contacts: " + std::string(e.what()));
    }
}

void MainWindow::update_status()
{
    int count = m_db->get_contact_count();
    m_status_label.set_text("Total contacts: " + std::to_string(count));
}

//-------------------- Dialogs --------------------

void MainWindow::show_error(const std::string& message)
{
    auto* dialog = new Gtk::MessageDialog(*this, message,
                                          false, Gtk::MessageType::ERROR,
                                          Gtk::ButtonsType::OK);
    dialog->set_modal(true);

    dialog->signal_response().connect([dialog](int){
        dialog->close();
        delete dialog;
    });

    dialog->present();
}

void MainWindow::show_info(const std::string& message)
{
    auto* dialog = new Gtk::MessageDialog(*this, message,
                                          false, Gtk::MessageType::INFO,
                                          Gtk::ButtonsType::OK);
    dialog->set_modal(true);

    dialog->signal_response().connect([dialog](int){
        dialog->close();
        delete dialog;
    });

    dialog->present();
}

//-------------------- Helpers --------------------

std::optional<int> MainWindow::get_selected_id()
{
    auto selection = m_tree_view.get_selection();
    if (!selection) return std::nullopt;

    auto iter = selection->get_selected();
    if (!iter) return std::nullopt;

    return (*iter)[m_columns.col_id];
}
