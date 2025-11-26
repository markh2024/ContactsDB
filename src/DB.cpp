#include "DB.hpp"
#include <iostream>
#include <regex>
#include <algorithm>

// -----------------------------
// Constructor
// -----------------------------
DB::DB(const std::string& host,
       const std::string& user,
       const std::string& password,
       const std::string& db_name,
       unsigned int port)
{
    try {
        // Build connection string
        std::string conn_string = "jdbc:mariadb://" + host + ":" + std::to_string(port) + "/" + db_name;
        
        sql::Properties connection_properties;
        connection_properties["user"] = user;
        connection_properties["password"] = password;

        conn_ = std::shared_ptr<sql::Connection>(
            sql::mariadb::get_driver_instance()->connect(conn_string, connection_properties)
        );
        
        if (!conn_ || !conn_->isValid()) {
            throw DBException("Failed to establish database connection");
        }
        
        std::cout << "Database connected successfully\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Database connection error: " + std::string(e.what()));
    }
}

// -----------------------------
// Test connection
// -----------------------------
bool DB::test_connection()
{
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement("SELECT 1")
        );
        stmt->executeQuery();
        return true;
    }
    catch (const sql::SQLException&) {
        return false;
    }
}

// -----------------------------
// Initialize schema
// -----------------------------
void DB::initialize_schema()
{
    try {
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "CREATE TABLE IF NOT EXISTS contacts ("
                "id INT AUTO_INCREMENT PRIMARY KEY, "
                "first_name VARCHAR(100), "
                "last_name VARCHAR(100), "
                "email VARCHAR(255), "
                "mobile VARCHAR(50), "
                "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
                "INDEX idx_name (first_name, last_name), "
                "INDEX idx_email (email)"
                ")"
            )
        );
        stmt->execute();
        std::cout << "Database schema initialized\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Schema initialization error: " + std::string(e.what()));
    }
}

// -----------------------------
// Insert contact
// -----------------------------
void DB::insert_contact(const std::string& first,
                        const std::string& last,
                        const std::string& email,
                        const std::string& mobile)
{
    if (first.empty() && last.empty()) {
        throw DBException("At least first name or last name must be provided");
    }
    
    if (!email.empty() && !is_valid_email(email)) {
        throw DBException("Invalid email format");
    }
    
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "INSERT INTO contacts (first_name,last_name,email,mobile) VALUES (?,?,?,?)"
            )
        );

        stmt->setString(1, first);
        stmt->setString(2, last);
        stmt->setString(3, email);
        stmt->setString(4, mobile);

        stmt->executeUpdate();
        std::cout << "Inserted contact: " << first << " " << last << "\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Insert error: " + std::string(e.what()));
    }
}

// -----------------------------
// Update contact
// -----------------------------
void DB::update_contact(int id,
                        const std::string& first,
                        const std::string& last,
                        const std::string& email,
                        const std::string& mobile)
{
    if (first.empty() && last.empty()) {
        throw DBException("At least first name or last name must be provided");
    }
    
    if (!email.empty() && !is_valid_email(email)) {
        throw DBException("Invalid email format");
    }
    
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "UPDATE contacts SET first_name=?, last_name=?, email=?, mobile=? WHERE id=?"
            )
        );

        stmt->setString(1, first);
        stmt->setString(2, last);
        stmt->setString(3, email);
        stmt->setString(4, mobile);
        stmt->setInt(5, id);

        int rows = stmt->executeUpdate();
        if (rows == 0) {
            throw DBException("Contact not found with ID: " + std::to_string(id));
        }
        std::cout << "Updated contact ID: " << id << "\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Update error: " + std::string(e.what()));
    }
}

// -----------------------------
// Delete contact
// -----------------------------
void DB::delete_contact(int id)
{
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement("DELETE FROM contacts WHERE id=?")
        );

        stmt->setInt(1, id);
        int rows = stmt->executeUpdate();
        if (rows == 0) {
            throw DBException("Contact not found with ID: " + std::to_string(id));
        }
        std::cout << "Deleted contact ID: " << id << "\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Delete error: " + std::string(e.what()));
    }
}

// -----------------------------
// Get contact by ID
// -----------------------------
std::optional<Contact> DB::get_contact_by_id(int id)
{
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "SELECT id, first_name, last_name, email, mobile FROM contacts WHERE id=?"
            )
        );
        stmt->setInt(1, id);

        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        if (res->next()) {
            Contact c{
                res->getInt("id"),
                res->getString("first_name").c_str(),
                res->getString("last_name").c_str(),
                res->getString("email").c_str(),
                res->getString("mobile").c_str()
            };
            return c;
        }
    }
    catch (const sql::SQLException& e) {
        std::cerr << "Query error: " << e.what() << "\n";
    }
    return std::nullopt;
}

// -----------------------------
// Get all contacts
// -----------------------------
std::vector<Contact> DB::get_all_contacts() const
{
    std::vector<Contact> contacts;
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "SELECT id, first_name, last_name, email, mobile FROM contacts ORDER BY last_name, first_name"
            )
        );

        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        while (res->next()) {
            contacts.push_back(Contact{
                res->getInt("id"),
                res->getString("first_name").c_str(),
                res->getString("last_name").c_str(),
                res->getString("email").c_str(),
                res->getString("mobile").c_str()
            });
        }
    }
    catch (const sql::SQLException& e) {
        std::cerr << "Query error: " << e.what() << "\n";
    }
    return contacts;
}

// -----------------------------
// Search contacts
// -----------------------------
std::vector<Contact> DB::search_contacts(const std::string& query) const
{
    std::vector<Contact> contacts;
    if (query.empty()) {
        return get_all_contacts();
    }
    
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "SELECT id, first_name, last_name, email, mobile FROM contacts "
                "WHERE first_name LIKE ? OR last_name LIKE ? OR email LIKE ? OR mobile LIKE ? "
                "ORDER BY last_name, first_name"
            )
        );
        
        std::string search_pattern = "%" + query + "%";
        stmt->setString(1, search_pattern);
        stmt->setString(2, search_pattern);
        stmt->setString(3, search_pattern);
        stmt->setString(4, search_pattern);

        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        while (res->next()) {
            contacts.push_back(Contact{
                res->getInt("id"),
                res->getString("first_name").c_str(),
                res->getString("last_name").c_str(),
                res->getString("email").c_str(),
                res->getString("mobile").c_str()
            });
        }
    }
    catch (const sql::SQLException& e) {
        std::cerr << "Search error: " << e.what() << "\n";
    }
    return contacts;
}

// -----------------------------
// Get contacts sorted
// -----------------------------
std::vector<Contact> DB::get_contacts_sorted(const std::string& column, bool ascending) const
{
    std::vector<Contact> contacts;
    try {
        ensure_connection();
        std::string safe_column = sanitize_column_name(column);
        std::string order = ascending ? "ASC" : "DESC";
        
        std::string query = "SELECT id, first_name, last_name, email, mobile FROM contacts "
                           "ORDER BY " + safe_column + " " + order;
        
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(query)
        );

        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        while (res->next()) {
            contacts.push_back(Contact{
                res->getInt("id"),
                res->getString("first_name").c_str(),
                res->getString("last_name").c_str(),
                res->getString("email").c_str(),
                res->getString("mobile").c_str()
            });
        }
    }
    catch (const sql::SQLException& e) {
        std::cerr << "Sort error: " << e.what() << "\n";
    }
    return contacts;
}

// -----------------------------
// Get contact count
// -----------------------------
int DB::get_contact_count() const
{
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement("SELECT COUNT(*) as count FROM contacts")
        );
        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        if (res->next()) {
            return res->getInt("count");
        }
    }
    catch (const sql::SQLException& e) {
        std::cerr << "Count error: " << e.what() << "\n";
    }
    return 0;
}

// -----------------------------
// Delete all contacts
// -----------------------------
void DB::delete_all_contacts()
{
    try {
        ensure_connection();
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement("DELETE FROM contacts")
        );
        stmt->executeUpdate();
        std::cout << "All contacts deleted\n";
    }
    catch (const sql::SQLException& e) {
        throw DBException("Delete all error: " + std::string(e.what()));
    }
}

// -----------------------------
// Import contacts
// -----------------------------
bool DB::import_contacts(const std::vector<Contact>& contacts)
{
    try {
        ensure_connection();
        conn_->setAutoCommit(false);
        
        auto stmt = std::unique_ptr<sql::PreparedStatement>(
            conn_->prepareStatement(
                "INSERT INTO contacts (first_name,last_name,email,mobile) VALUES (?,?,?,?)"
            )
        );
        
        for (const auto& c : contacts) {
            stmt->setString(1, c.first_name);
            stmt->setString(2, c.last_name);
            stmt->setString(3, c.email);
            stmt->setString(4, c.mobile);
            stmt->executeUpdate();
        }
        
        conn_->commit();
        conn_->setAutoCommit(true);
        std::cout << "Imported " << contacts.size() << " contacts\n";
        return true;
    }
    catch (const sql::SQLException& e) {
        conn_->rollback();
        conn_->setAutoCommit(true);
        std::cerr << "Import error: " << e.what() << "\n";
        return false;
    }
}

// -----------------------------
// Email validation
// -----------------------------
bool DB::is_valid_email(const std::string& email)
{
    const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, pattern);
}

// -----------------------------
// Private helpers
// -----------------------------
void DB::ensure_connection() const
{
    if (!conn_ || !conn_->isValid()) {
        throw DBException("Database connection lost");
    }
}

std::string DB::sanitize_column_name(const std::string& column) const
{
    static const std::vector<std::string> valid_columns = {
        "id", "first_name", "last_name", "email", "mobile"
    };
    
    if (std::find(valid_columns.begin(), valid_columns.end(), column) != valid_columns.end()) {
        return column;
    }
    return "last_name"; // default
}
