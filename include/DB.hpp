#pragma once

#include <mariadb/conncpp.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <stdexcept>

struct Contact {
    int id;
    std::string first_name;
    std::string last_name;
    std::string email;
    std::string mobile;
    
    // Validation helper
    bool is_valid() const {
        return !first_name.empty() || !last_name.empty();
    }
};

class DBException : public std::runtime_error {
public:
    explicit DBException(const std::string& msg) : std::runtime_error(msg) {}
};

class DB {
public:
    // Constructor
    DB(const std::string& host,
       const std::string& user,
       const std::string& password,
       const std::string& db_name,
       unsigned int port = 3306);

    // Test connection
    bool test_connection();
    
    // Initialize database schema
    void initialize_schema();

    // CRUD operations
    void insert_contact(const std::string& first,
                        const std::string& last,
                        const std::string& email,
                        const std::string& mobile);

    void update_contact(int id,
                        const std::string& first,
                        const std::string& last,
                        const std::string& email,
                        const std::string& mobile);

    void delete_contact(int id);

    std::optional<Contact> get_contact_by_id(int id);
    std::vector<Contact> get_all_contacts() const;
    
    // Search and filter
    std::vector<Contact> search_contacts(const std::string& query) const;
    std::vector<Contact> get_contacts_sorted(const std::string& column, bool ascending = true) const;
    
    // Statistics
    int get_contact_count() const;
    
    // Bulk operations
    void delete_all_contacts();
    bool import_contacts(const std::vector<Contact>& contacts);
    
    // Email validation helper
    static bool is_valid_email(const std::string& email);

private:
    mutable std::shared_ptr<sql::Connection> conn_;
    
    void ensure_connection() const;
    std::string sanitize_column_name(const std::string& column) const;
};
