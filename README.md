# AppointC

AppointC is a console-based appointment management system developed in C.  
The application allows users to manage appointments efficiently through an interactive terminal interface.

The project was designed to practice data structures, file handling, dynamic memory allocation, and modular programming in C.

---

# Features

- Appointment creation and management
- Linked list implementation using dynamic memory allocation
- Sorted insertion by appointment date
- Search appointments by:
  - ID
  - Name
  - Date
- Modify existing appointments
- Delete appointments
- Persistent storage using text files (`rdv.txt`)
- Input validation and structured console interface

---

# Technologies

- C Programming Language
- Standard C Libraries
  - `stdio.h`
  - `stdlib.h`
  - `string.h`

---

# Data Persistence

Appointments are automatically stored in:

```text
rdv.txt
```

This allows data to remain available after restarting the program.

---

# Project Structure

```text
AppointC/
│
├── appointc.c
├── rdv.txt
├── rdv_sample.txt
└── README.md
```

---

# Compilation

Using GCC:

```bash
gcc appointc.c -o appointc
```

---

# Execution

```bash
./appointc
```

---

# Test Data

To test the application with sample data:

```text
Rename rdv_sample.txt to rdv.txt
```

---

# Concepts Practiced

This project demonstrates:

- Linked lists
- Dynamic memory management
- File handling in C
- Searching and sorting algorithms
- CRUD operations
- Console application development

---

# Future Improvements

- Graphical interface
- Authentication system
- Appointment reminders
- Database integration
- Calendar visualization

---

# Author

Redallah Tarkaoui
