# Contacts Database Application - Architecture Documentation

## Overview

This is a GTK4/gtkmm4 desktop application for managing contacts stored in a MariaDB database. The application uses modern C++ (C++17) with smart pointers and RAII principles for memory management.

---

## Architecture Diagram

```
┌─────────────────┐
│   main.cpp      │ (Application Entry Point)
└────────┬────────┘
         │ creates
         ▼
┌─────────────────────────┐
│ ContactsApplication     │ (Gtk::Application)
│ - Manages app lifecycle │
└────────┬────────────────┘
         │ shows
         ▼
┌─────────────────────────┐        ┌──────────────┐
│ DBConnectionDialog      │───────>│   DB.hpp     │
│ - Get DB credentials    │ tests  │ - Database   │
└────────┬────────────────┘        │   Layer      │
         │ success                 └──────┬───────┘
         ▼                                │
┌─────────────────────────┐              │
│   MainWindow            │──────────────┘
│ - Main UI               │   uses
│ - TreeView              │
└────────┬────────────────┘
         │ opens
         ▼
┌─────────────────────────┐
│   ContactDialog         │
│ - Add/Edit contacts     │
└─────────────────────────┘
```

---

## File-by-File Breakdown

### 1. **main.cpp** - Application Entry Point

#### Purpose
Bootstrap the application, manage the database connection flow, and handle top-level error dialogs.

#### Key Components

**ContactsApplication Class**
```cpp
class ContactsApplication : public Gtk::Application
```
- Inherits from `Gtk::Application` (GTK4's application framework)
- Manages the application lifecycle
- Handles the connection dialog → main window flow

**Key Methods:**

**`on_activate()`**
```cpp
void on_activate() override
```
- Called when the application starts
- Shows the database connection dialog
- This is the GTK application's entry point

**`show_connection_dialog()`**
```cpp
void show_connection_dialog()
```
- Creates a heap-allocated `DBConnectionDialog` (using `new`)
- Passes a lambda callback that will be invoked on successful connection
- The callback:
  1. Creates the `DB` object (wrapped in `shared_ptr`)
  2. Tests the connection
  3. Creates and shows the `MainWindow`
  4. Handles any connection errors

**Memory Management Pattern:**
```cpp
auto* dialog = new DBConnectionDialog([this](...) {
    // Lambda captures 'this' to access member variables
    m_db = std::make_shared<DB>(...);
    auto* window = new MainWindow(m_db);
    add_window(*window);
});

dialog->signal_hide().connect([dialog]() { 
    delete dialog;  // Cleanup when dialog closes
});
```

**Why heap allocation?**
- GTK4 widgets must outlive the function that creates them
- Stack-allocated objects would be destroyed when the function returns
- The `signal_hide()` connection ensures proper cleanup

**Shared Pointer Usage:**
```cpp
std::shared_ptr<DB> m_db;
```
- `shared_ptr` allows multiple windows to share the same DB connection
- Automatic cleanup when the last reference is destroyed
- Thread-safe reference counting

---

### 2. **DB.hpp / DB.cpp** - Database Layer

#### Purpose
Encapsulate all database operations, provide a clean interface for CRUD operations, and handle MariaDB connection management.

#### Key Components

**Contact Struct**
```cpp
struct Contact {
    int id;
    std::string first_name;
    std::string last_name;
    std::string email;
    std::string mobile;
    
    bool is_valid() const {
        return !first_name.empty() || !last_name.empty();
    }
};
```
- Plain Old Data (POD) structure
- Represents a single contact record
- Validation helper ensures at least one name is provided

**DB Class**
```cpp
class DB {
private:
    mutable std::shared_ptr<sql::Connection> conn_;
    
    void ensure_connection() const;
    std::string sanitize_column_name(const std::string& column) const;
};
```

**Why `mutable`?**
```cpp
mutable std::shared_ptr<sql::Connection> conn_;
```
- Allows modification in `const` methods
- `const` methods like `get_all_contacts()` need to check/refresh connection
- Connection state is logically separate from data operations
- This is a common pattern for caching/connection management

**Key Methods:**

**Constructor**
```cpp
DB::DB(const std::string& host,
       const std::string& user,
       const std::string& password,
       const std::string& db_name,
       unsigned int port)
```
- Creates MariaDB connection using connection string
- Throws `DBException` on failure
- Validates connection immediately

**ensure_connection()**
```cpp
void DB::ensure_connection() const
```
- Marked `const` because it doesn't change logical state
- Validates that connection is still alive
- Throws `DBException` if connection lost
- Called before every database operation

**Smart Pointer Pattern:**
```cpp
auto stmt = std::unique_ptr<sql::PreparedStatement>(
    conn_->prepareStatement("SELECT ...")
);
```
- `unique_ptr` manages prepared statement lifetime
- Automatic cleanup when function exits
- Exception-safe (RAII principle)
- No manual `delete` needed

**Prepared Statements (SQL Injection Prevention):**
```cpp
auto stmt = conn_->prepareStatement(
    "INSERT INTO contacts (first_name,last_name,email,mobile) VALUES (?,?,?,?)"
);
stmt->setString(1, first);
stmt->setString(2, last);
stmt->setString(3, email);
stmt->setString(4, mobile);
```
- `?` placeholders prevent SQL injection
- Values are properly escaped by the driver
- Never concatenate user input into SQL strings

**Transaction Pattern (Bulk Import):**
```cpp
bool DB::import_contacts(const std::vector<Contact>& contacts)
{
    try {
        conn_->setAutoCommit(false);  // Start transaction
        
        // ... multiple INSERT operations ...
        
        conn_->commit();               // Commit all
        conn_->setAutoCommit(true);
        return true;
    }
    catch (const sql::SQLException& e) {
        conn_->rollback();            // Rollback on error
        conn_->setAutoCommit(true);
        return false;
    }
}
```
- All-or-nothing import
- Maintains database integrity
- Rolls back on any error

---

### 3. **DBConnectionDialog.hpp / DBConnectionDialog.cpp** - Connection Dialog

#### Purpose
Present a UI for entering database credentials, test the connection, and save credentials for convenience.

#### Key Components

**Class Declaration**
```cpp
class DBConnectionDialog : public Gtk::Window
```
- Inherits from `Gtk::Window` (top-level window)
- Modal dialog for database connection

**Callback Pattern:**
```cpp
std::function<void(const std::string&, unsigned int, 
                   const std::string&, const std::string&)> m_on_connect;
```
- `std::function` holds any callable (lambda, function pointer, functor)
- Called when user clicks "Connect" with validated credentials
- Decouples dialog from application logic

**Widget Members:**
```cpp
Gtk::Box m_main_box{Gtk::Orientation::VERTICAL};
Gtk::Grid grid;
Gtk::Entry entry_host;
Gtk::Entry entry_port;
// ... etc
```
- Widgets are stack-allocated members (not pointers)
- Automatic lifetime management
- Destroyed when dialog is destroyed

**Key Methods:**

**Constructor**
```cpp
DBConnectionDialog::DBConnectionDialog(
    std::function<void(...)> on_connect)
```
- Stores the callback
- Builds the UI layout
- Loads saved credentials if available
- Sets up signal connections

**Signal Connection Pattern:**
```cpp
ok_button.signal_clicked().connect([this](){ on_ok(); });
```
- Lambda captures `this` to access member methods
- `signal_clicked()` returns a signal object
- `.connect()` registers the callback
- Automatic disconnection when dialog destroyed

**on_ok() - Validation Flow**
```cpp
void DBConnectionDialog::on_ok()
{
    // 1. Get form values
    std::string host = entry_host.get_text();
    
    // 2. Validate input
    if (host.empty() || port_str.empty() || user.empty()) {
        show_status("Please fill in all required fields", true);
        return;  // Don't close, stay open for correction
    }
    
    // 3. Parse and validate port
    try {
        port = std::stoi(port_str);
        if (port == 0 || port > 65535)
            throw std::out_of_range("Port out of range");
    } catch (...) {
        show_status("Invalid port number", true);
        return;
    }
    
    // 4. Save credentials if requested
    if (check_remember.get_active())
        save_credentials();
    
    // 5. Call the callback with credentials
    if (m_on_connect)
        m_on_connect(host, port, user, password);
    
    // 6. Hide dialog (triggers signal_hide in main.cpp)
    hide();
}
```

**Credentials Storage:**
```cpp
void DBConnectionDialog::save_credentials()
{
    std::string config_dir = std::string(getenv("HOME")) 
                           + "/.config/contacts-app";
    std::filesystem::create_directories(config_dir);
    
    std::ofstream file(config_file);
    file << entry_host.get_text() << "\n";
    // ... write other fields ...
    
    // Secure file permissions (owner only)
    std::filesystem::permissions(config_file, 
        std::filesystem::perms::owner_read | 
        std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
}
```
- Stores in user's config directory
- **Note:** Passwords are NOT saved (security)
- File permissions set to 0600 (owner read/write only)

---

### 4. **MainWindow.hpp / MainWindow.cpp** - Main Application Window

#### Purpose
Display the contacts list, provide search/filter functionality, and manage CRUD operations through dialogs.

#### Key Components

**Class Declaration**
```cpp
class MainWindow : public Gtk::ApplicationWindow
```
- `ApplicationWindow` is a top-level window managed by `Gtk::Application`
- Automatically handles application lifecycle

**Database Reference:**
```cpp
std::shared_ptr<DB> m_db;
```
- Shares ownership of the DB connection
- Passed from `main.cpp` via constructor
- Multiple windows could share the same connection

**TreeView Model Pattern:**
```cpp
struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
    ModelColumns() {
        add(col_id); 
        add(col_first); 
        add(col_last);
        add(col_email); 
        add(col_mobile);
    }
    Gtk::TreeModelColumn<int> col_id;
    Gtk::TreeModelColumn<std::string> col_first;
    // ... etc
};

ModelColumns m_columns;
Glib::RefPtr<Gtk::ListStore> m_ref_list_store;
Gtk::TreeView m_tree_view;
```

**What's happening here?**

1. **ModelColumns**: Defines the structure (schema) of the data
2. **ListStore**: The actual data container (like a table in memory)
3. **TreeView**: The visual widget that displays the data

**RefPtr Explained:**
```cpp
Glib::RefPtr<Gtk::ListStore> m_ref_list_store;
```
- `RefPtr` is GTK's reference-counted smart pointer
- Similar to `shared_ptr` but for GTK objects
- Automatically manages GTK object lifecycle
- Use `.get()` to get raw pointer when needed

**Layout Structure:**
```
MainWindow
└── m_main_box (Vertical Box)
    ├── m_toolbar_box (Search bar)
    │   ├── Search label
    │   ├── m_search_entry
    │   └── m_clear_search_button
    ├── m_scrolled_window
    │   └── m_tree_view (Contacts list)
    ├── m_button_box (Action buttons)
    │   ├── Add, Edit, Delete
    │   └── Import, Export
    └── m_status_box (Status label)
```

**Key Methods:**

**Constructor**
```cpp
MainWindow::MainWindow(std::shared_ptr<DB> db)
: m_db(db)
{
    // 1. Initialize database schema
    m_db->initialize_schema();
    
    // 2. Create and configure tree view
    m_ref_list_store = Gtk::ListStore::create(m_columns);
    m_tree_view.set_model(m_ref_list_store);
    setup_tree_view_columns();
    
    // 3. Connect signals
    m_add_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_add_contact)
    );
    
    // 4. Load initial data
    refresh_list();
    update_status();
}
```

**sigc::mem_fun Explained:**
```cpp
sigc::mem_fun(*this, &MainWindow::on_add_contact)
```
- Part of libsigc++ (signal/slot library)
- Creates a callback that calls a member function
- `*this` = the object to call the method on
- `&MainWindow::on_add_contact` = pointer to member function
- Equivalent to: `[this]() { on_add_contact(); }`

**Dialog Creation Pattern:**
```cpp
void MainWindow::on_add_contact()
{
    // 1. Create dialog on heap (must outlive this function)
    auto* dialog = new ContactDialog(*this, *m_db,
        [this](){ 
            refresh_list();    // Refresh when saved
            update_status(); 
        },
        false,  // Not editing
        0       // No contact ID
    );
    
    // 2. Make it modal (blocks interaction with parent)
    dialog->set_modal(true);

    // 3. Setup cleanup when dialog closes
    dialog->signal_hide().connect([dialog](){
        delete dialog;  // Free memory
    });

    // 4. Show the dialog
    dialog->present();
}
```

**Why this pattern?**
- Dialog must be heap-allocated (needs to outlive function)
- `signal_hide()` ensures cleanup after user closes dialog
- Lambda callback refreshes the list when contact is saved
- Modal prevents user from interacting with main window while editing

**Refresh Pattern:**
```cpp
void MainWindow::refresh_list()
{
    m_ref_list_store->clear();  // Clear existing rows
    
    std::vector<Contact> contacts;
    
    // Get contacts (search or all)
    if (!m_current_search.empty())
        contacts = m_db->search_contacts(m_current_search);
    else
        contacts = m_db->get_all_contacts();
    
    // Populate tree view
    for (const auto& c : contacts) {
        auto row = *(m_ref_list_store->append());
        row[m_columns.col_id] = c.id;
        row[m_columns.col_first] = c.first_name;
        row[m_columns.col_last] = c.last_name;
        row[m_columns.col_email] = c.email;
        row[m_columns.col_mobile] = c.mobile;
    }
}
```

**CSV Import Flow:**
```cpp
void MainWindow::on_import_csv()
{
    // 1. Create file chooser dialog
    auto* dialog = new Gtk::FileChooserDialog(...);
    
    // 2. Add CSV filter
    auto filter = Gtk::FileFilter::create();
    filter->add_pattern("*.csv");
    dialog->add_filter(filter);
    
    // 3. Handle response
    dialog->signal_response().connect([this, dialog](int response_id){
        if (response_id == Gtk::ResponseType::ACCEPT) {
            auto file = dialog->get_file();
            
            // Parse CSV
            std::ifstream infile(file->get_path());
            // ... parse lines ...
            
            // Bulk import
            m_db->import_contacts(contacts);
            refresh_list();
        }
        delete dialog;
    });
    
    dialog->present();
}
```

---

### 5. **ContactDialogs.hpp / ContactDialogs.cpp** - Add/Edit Contact Dialog

#### Purpose
Provide a form for adding new contacts or editing existing ones, with validation.

#### Key Components

**Class Declaration**
```cpp
class ContactDialog : public Gtk::Dialog
```
- Inherits from `Gtk::Dialog` (modal dialog base class)
- Handles both Add and Edit modes

**Member Variables:**
```cpp
DB& m_db;                        // Reference to database
std::function<void()> m_on_saved; // Callback when saved
bool m_editing;                  // Add vs Edit mode
int m_contact_id;                // ID when editing
```

**Why reference vs pointer?**
```cpp
DB& m_db;  // Reference (always valid, can't be null)
```
- References must be initialized in constructor
- Cannot be null (safer than pointer)
- Cannot be reassigned (immutable binding)
- Preferred when object is guaranteed to exist

**Callback Pattern:**
```cpp
std::function<void()> m_on_saved;
```
- Called after successful save
- MainWindow passes: `[this](){ refresh_list(); }`
- Allows dialog to notify parent without tight coupling

**Key Methods:**

**Constructor - Dual Mode:**
```cpp
ContactDialog::ContactDialog(Gtk::Window& parent, DB& db,
                             std::function<void()> on_saved,
                             bool editing, int contact_id)
{
    // Set title based on mode
    set_title(m_editing ? "Edit Contact" : "Add Contact");
    
    // Set as modal child of parent
    set_modal(true);
    set_transient_for(parent);
    
    // ... build UI ...
    
    // Load data if editing
    if (m_editing)
        load_contact_data();
}
```

**Transient Window:**
```cpp
set_transient_for(parent);
```
- Makes this dialog a "child" of parent window
- Dialog moves with parent
- Parent is dimmed/disabled when modal
- Dialog is destroyed when parent closes

**Layout Building:**
```cpp
// Get dialog's content area
auto* area = get_content_area();
area->append(m_main_box);

// Use a Grid for form layout
grid.set_row_spacing(10);
grid.set_column_spacing(12);

// Labels aligned to the right
auto* lbl_first = Gtk::make_managed<Gtk::Label>("First Name:");
lbl_first->set_halign(Gtk::Align::END);

// Entries expand horizontally
entry_first.set_hexpand(true);
entry_first.set_placeholder_text("Enter first name");

// Attach to grid: widget, column, row, width, height
grid.attach(*lbl_first, 0, 0);
grid.attach(entry_first, 1, 0);
```

**Gtk::make_managed:**
```cpp
auto* lbl_first = Gtk::make_managed<Gtk::Label>("First Name:");
```
- Creates a GTK widget managed by its parent container
- Parent (`grid`) owns the widget
- Automatic cleanup when parent is destroyed
- Don't `delete` managed widgets

**Validation Pattern:**
```cpp
bool ContactDialog::validate_input()
{
    std::string first = trim(entry_first.get_text());
    std::string last = trim(entry_last.get_text());
    std::string email = trim(entry_email.get_text());
    
    // At least one name required
    if (first.empty() && last.empty()) {
        show_validation_error("Please enter at least a first name or last name");
        return false;
    }
    
    // Email validation if provided
    if (!email.empty() && !DB::is_valid_email(email)) {
        show_validation_error("Please enter a valid email address");
        return false;
    }
    
    clear_validation_error();
    return true;
}
```

**Save Flow:**
```cpp
void ContactDialog::on_ok_clicked()
{
    // 1. Validate input
    if (!validate_input()) {
        return;  // Stay open, show error
    }
    
    // 2. Trim values
    std::string first = trim(entry_first.get_text());
    std::string last = trim(entry_last.get_text());
    std::string email = trim(entry_email.get_text());
    std::string mobile = trim(entry_mobile.get_text());
    
    // 3. Save to database
    try {
        if (m_editing) {
            m_db.update_contact(m_contact_id, first, last, email, mobile);
        } else {
            m_db.insert_contact(first, last, email, mobile);
        }

        // 4. Notify parent (refresh list)
        if (m_on_saved)
            m_on_saved();
            
        // 5. Close dialog
        hide();  // Triggers signal_hide() in MainWindow
    }
    catch (const DBException& e) {
        show_validation_error(e.what());
        // Don't close on error
    }
}
```

**String Trimming:**
```cpp
std::string ContactDialog::trim(const std::string& str)
{
    if (str.empty())
        return str;
        
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";  // String is all whitespace
        
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}
```
- Removes leading and trailing whitespace
- Handles empty strings and all-whitespace strings
- Returns a new string (doesn't modify original)

---

## Memory Management Patterns

### 1. **Stack vs Heap Allocation**

**Stack (Automatic):**
```cpp
Gtk::Button m_add_button{"Add"};  // Member variable
```
- Destroyed when owner is destroyed
- Fast allocation/deallocation
- Fixed size known at compile time

**Heap (Dynamic):**
```cpp
auto* dialog = new ContactDialog(...);
```
- Must outlive the function that creates it
- Explicit cleanup required
- Size can be determined at runtime

### 2. **Smart Pointers**

**`std::shared_ptr`** - Shared Ownership:
```cpp
std::shared_ptr<DB> m_db;
```
- Multiple owners
- Reference counted
- Deleted when last owner is destroyed
- Thread-safe

**`std::unique_ptr`** - Exclusive Ownership:
```cpp
auto stmt = std::unique_ptr<sql::PreparedStatement>(...);
```
- Single owner
- Cannot be copied (only moved)
- Zero overhead compared to raw pointer
- Exception-safe (RAII)

**`Glib::RefPtr`** - GTK Reference Counting:
```cpp
Glib::RefPtr<Gtk::ListStore> m_ref_list_store;
```
- GTK's reference counting system
- Similar to `shared_ptr`
- Required for GTK objects that use reference counting

### 3. **Raw Pointers (When Required)**

**GTK Widget Creation:**
```cpp
auto* label = Gtk::make_managed<Gtk::Label>("Text");
```
- Returns raw pointer
- Parent container owns it
- Don't delete manually

**Heap Dialog Pattern:**
```cpp
auto* dialog = new ContactDialog(...);
dialog->signal_hide().connect([dialog](){
    delete dialog;  // Manual cleanup
});
```
- Required for GTK dialogs
- `signal_hide()` ensures proper timing
- Lambda captures pointer for deletion

---

## Signal/Slot Pattern

### How GTK Signals Work

```cpp
button.signal_clicked().connect([this]() {
    on_button_clicked();
});
```

1. `signal_clicked()` returns a signal object
2. `.connect()` registers a callback
3. When button clicked, signal emits
4. All connected callbacks are invoked
5. Automatic disconnection when widget destroyed

### Lambda Captures

**`[this]`** - Capture the object pointer:
```cpp
m_button.signal_clicked().connect([this]() {
    this->on_button_clicked();  // Can call member methods
});
```

**`[dialog]`** - Capture a pointer by value:
```cpp
dialog->signal_hide().connect([dialog]() {
    delete dialog;  // Can access the captured pointer
});
```

**`[&]`** - Capture everything by reference (dangerous):
```cpp
// DON'T DO THIS - variables may not exist when callback runs
int count = 0;
button.signal_clicked().connect([&]() {
    count++;  // DANGER: count might be out of scope
});
```

---

## Exception Safety

### Try-Catch Blocks

```cpp
try {
    m_db->insert_contact(first, last, email, mobile);
    refresh_list();
}
catch (const DBException& e) {
    show_error(e.what());  // Handle specific exception
}
catch (const std::exception& e) {
    show_error(e.what());  // Handle any standard exception
}
```

### RAII (Resource Acquisition Is Initialization)

```cpp
{
    std::unique_ptr<sql::PreparedStatement> stmt(...);
    // Use stmt
    // ...
}  // stmt automatically deleted here, even if exception thrown
```

Benefits:
- No memory leaks even if exception occurs
- No need for manual cleanup
- Automatic, predictable resource management

---

## Database Connection Flow

```
1. User opens app
   └─> ContactsApplication::on_activate()

2. Show connection dialog
   └─> DBConnectionDialog created

3. User enters credentials and clicks "Connect"
   └─> DBConnectionDialog::on_ok()
       └─> Validates input
       └─> Calls m_on_connect callback

4. Callback in main.cpp
   └─> Creates DB object
   └─> Tests connection
   └─> Creates MainWindow
   └─> Passes shared_ptr<DB> to MainWindow

5. MainWindow initializes
   └─> Calls m_db->initialize_schema()
   └─> Loads contacts with refresh_list()

6. User adds/edits contact
   └─> ContactDialog created
   └─> Shares DB reference
   └─> Validates and saves
   └─> Calls callback to refresh MainWindow

7. All operations use the same DB connection
   └─> Connection checked before each query
   └─> Exceptions propagate to UI for display
```

---

## Best Practices Demonstrated

### 1. **Separation of Concerns**
- DB layer handles all database logic
- UI classes handle only display and user interaction
- No SQL queries in UI code

### 2. **Callback Pattern**
- Dialogs don't know about MainWindow internals
- They just call `m_on_saved()` callback
- Loose coupling, high cohesion

### 3. **Const Correctness**
```cpp
std::vector<Contact> get_all_contacts() const;
```
- Methods that don't modify state are marked `const`
- Compiler enforces this guarantee
- Safer, more predictable code

### 4. **RAII and Smart Pointers**
- No manual memory management
- Exception-safe by default
- Clear ownership semantics

### 5. **Input Validation**
- All user input validated before database operations
- SQL injection prevented with prepared statements
- Email validation with regex

### 6. **Error Handling**
- Exceptions for exceptional conditions (DB errors)
- User-friendly error messages
- No silent failures

---

## Common Patterns Reference

### Creating a Modal Dialog
```cpp
auto* dialog = new SomeDialog(*this, ...);
dialog->set_modal(true);
dialog->signal_hide().connect([dialog]() {
    delete dialog;
});
dialog->present();
```

### Database Query Pattern
```cpp
void DB::some_operation() const {
    ensure_connection();  // Validate connection
    
    auto stmt = std::unique_ptr<sql::PreparedStatement>(
        conn_->prepareStatement("SELECT ...")
    );
    
    // Set parameters
    stmt->setString(1, value);
    
    // Execute
    auto res = std::unique_ptr<sql::ResultSet>(
        stmt->executeQuery()
    );
    
    // Process results
    while (res->next()) {
        // ...
    }
}
```

### GTK Layout Pattern
```cpp
// Vertical box for main layout
Gtk::Box main_box{Gtk::Orientation::VERTICAL};

// Add widgets
main_box.append(widget1);
main_box.append(widget2);

// Set as window content
set_child(main_box);
```

### Callback Connection
```cpp
widget.signal_event().connect(
    sigc::mem_fun(*this, &MyClass::on_event)
);
```

---

## Summary

This application demonstrates modern C++ desktop development with:

- **GTK4** for cross-platform GUI
- **Smart pointers** for automatic memory management
- **RAII** for exception safety
- **Prepared statements** for SQL injection prevention
- **Signal/slot pattern** for event handling
- **Separation of concerns** for maintainable code
- **Proper resource management** with no memory leaks

The architecture is clean, maintainable, and follows best practices for both C++ and GTK development.