-- Create database if it doesn't exist
CREATE DATABASE IF NOT EXISTS Contacts;

-- Use the database
USE Contacts;

-- Create contacts table
CREATE TABLE IF NOT EXISTS contacts (
    id INT AUTO_INCREMENT PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    email VARCHAR(255),
    mobile VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_name (first_name, last_name),
    INDEX idx_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Grant privileges
GRANT ALL PRIVILEGES ON Contacts.* TO 'root'@'localhost';
FLUSH PRIVILEGES;

-- Show success message
SELECT 'Database initialized successfully!' AS Status;
