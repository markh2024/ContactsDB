#pragma once
#include <gtkmm.h>
#include <memory>
#include "DB.hpp"
#include "ContactDialogs.hpp"

class MainWindow : public Gtk::ApplicationWindow
{
public:
    MainWindow(std::shared_ptr<DB> db);

private:
    std::shared_ptr<DB> m_db;

    // Layout
    Gtk::Box m_main_box{Gtk::Orientation::VERTICAL};
    Gtk::Box m_toolbar_box{Gtk::Orientation::HORIZONTAL};
    Gtk::Box m_button_box{Gtk::Orientation::HORIZONTAL};
    Gtk::Box m_status_box{Gtk::Orientation::HORIZONTAL};
    
    // Search
    Gtk::SearchEntry m_search_entry;
    Gtk::Button m_clear_search_button{"Clear"};
    
    // Buttons
    Gtk::Button m_add_button{"Add"};
    Gtk::Button m_edit_button{"Edit"};
    Gtk::Button m_delete_button{"Delete"};
    Gtk::Button m_import_button{"Import CSV"};
    Gtk::Button m_export_button{"Export CSV"};
    
    // Status bar
    Gtk::Label m_status_label;
    
    // Scrolled window for tree view
    Gtk::ScrolledWindow m_scrolled_window;

    // TreeView model
    struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
        ModelColumns() {
            add(col_id); add(col_first); add(col_last);
            add(col_email); add(col_mobile);
        }
        Gtk::TreeModelColumn<int> col_id;
        Gtk::TreeModelColumn<std::string> col_first;
        Gtk::TreeModelColumn<std::string> col_last;
        Gtk::TreeModelColumn<std::string> col_email;
        Gtk::TreeModelColumn<std::string> col_mobile;
    };

    ModelColumns m_columns;
    Glib::RefPtr<Gtk::ListStore> m_ref_list_store;
    Gtk::TreeView m_tree_view;
    
    // Current search query
    std::string m_current_search;
    
    // Sort state
    std::string m_sort_column = "last_name";
    bool m_sort_ascending = true;

    // Event handlers
    void on_add_contact();
    void on_edit_contact();
    void on_delete_contact();
    void on_search_changed();
    void on_clear_search();
    void on_import_csv();
    void on_export_csv();
    void on_column_clicked(const std::string& column);
    void on_row_activated([[maybe_unused]] const Gtk::TreeModel::Path& path,
                      [[maybe_unused]] Gtk::TreeViewColumn* column);
    
    // Helpers
    void refresh_list();
    void update_status();
    void show_error(const std::string& message);
    void show_info(const std::string& message);
    std::optional<int> get_selected_id();
    void setup_tree_view_columns();
};
