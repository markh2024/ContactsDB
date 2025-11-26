#include <gtkmm.h>
#include "DBConnectionDialog.hpp"
#include "MainWindow.hpp"
#include "DB.hpp"
#include <memory>
#include <iostream>

class ContactsApplication : public Gtk::Application {
protected:
    ContactsApplication()
        : Gtk::Application("com.contacts.app") {}

    void on_activate() override {
        show_connection_dialog();
    }

    void show_connection_dialog() {
        // Heap-allocate dialog for GTKmm 4 async usage
        auto* dialog = new DBConnectionDialog([this](const std::string& host,
                                                     unsigned int port,
                                                     const std::string& user,
                                                     const std::string& password) {
            try {
                // Create DB instance
                m_db = std::make_shared<DB>(host, user, password, "Contacts", port);

                // Test connection
                if (!m_db->test_connection()) {
                    throw std::runtime_error("Connection test failed");
                }

                // Open main window
                auto* window = new MainWindow(m_db);
                add_window(*window);
                window->present();

                std::cout << "Application started successfully\n";
            }
            catch (const DBException& e) {
                show_error_dialog("Database Error", e.what(), [this](){
                    show_connection_dialog();
                });
            }
            catch (const std::exception& e) {
                show_error_dialog("Connection Error",
                                  "Failed to connect to database:\n" + std::string(e.what()),
                                  [this](){
                    show_connection_dialog();
                });
            }
        });

        add_window(*dialog);
        dialog->set_modal(true);

        // Delete dialog when closed
        dialog->signal_hide().connect([dialog]() { delete dialog; });

        dialog->present();
    }

    void show_error_dialog(const std::string& title,
                           const std::string& message,
                           std::function<void()> on_close = nullptr) {
        Gtk::Window* parent = nullptr;
        auto windows = get_windows();
        if (!windows.empty()) {
            parent = dynamic_cast<Gtk::Window*>(windows[0]);
        }

        Gtk::MessageDialog* err_dialog;
        if (parent) {
            err_dialog = new Gtk::MessageDialog(*parent, message,
                                                false, Gtk::MessageType::ERROR,
                                                Gtk::ButtonsType::OK);
        } else {
            err_dialog = new Gtk::MessageDialog(message,
                                                false, Gtk::MessageType::ERROR,
                                                Gtk::ButtonsType::OK);
        }

        err_dialog->set_title(title);
        err_dialog->set_modal(true);

        err_dialog->signal_hide().connect([err_dialog, on_close]() {
            if (on_close) on_close();
            delete err_dialog;
        });

        err_dialog->present();
    }

public:
    static Glib::RefPtr<ContactsApplication> create() {
        return Glib::RefPtr<ContactsApplication>(new ContactsApplication());
    }

private:
    std::shared_ptr<DB> m_db;
};

int main(int argc, char* argv[])
{
    try {
        auto app = ContactsApplication::create();
        return app->run(argc, argv);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
