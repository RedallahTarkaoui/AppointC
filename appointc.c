#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ======================= CONSTANTS ======================= */
#define MAX_TEXT 50     // Max length for text fields (name, location, etc.)
#define DATE_LEN 10     // Length of date string "YYYY-MM-DD"
#define FILE_NAME "rdv.txt" // Data storage file

/* ======================= COLOR HANDLING (Windows Specific) ======================= */
#ifdef _WIN32
#include <windows.h>

static HANDLE g_hConsole;   // Handle to the console window
static WORD   g_defaultAttr; // Stores original console colors to restore later

/* Initializes console handle and saves default color attributes */
static void console_init(void) {
    g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(g_hConsole, &info)) {
        g_defaultAttr = info.wAttributes;
    } else {
        // Fallback if info retrieval fails (standard white)
        g_defaultAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}

/* Sets the console text color */
static void set_color(WORD attr) {
    SetConsoleTextAttribute(g_hConsole, attr);
}

/* Resets console text color to default */
static void reset_color(void) {
    SetConsoleTextAttribute(g_hConsole, g_defaultAttr);
}

/* Predefined color constants for Windows */
#define COLOR_RED     (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_GREEN   (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_YELLOW  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_CYAN    (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)

#else
/* Non-Windows systems: Color functions do nothing (plain text output) */
static void console_init(void) {}
static void set_color(int attr) { (void)attr; }
static void reset_color(void) {}
#define COLOR_RED     0
#define COLOR_GREEN   0
#define COLOR_YELLOW  0
#define COLOR_CYAN    0
#endif

/* ======================= DATA STRUCTURE ======================= */
typedef struct Appointment {
    int id;
    char name[MAX_TEXT];
    char phone[MAX_TEXT];
    char date[DATE_LEN + 1];   /* Format: YYYY-MM-DD */
    char location[MAX_TEXT];
    char subject[MAX_TEXT];
    struct Appointment* next;  // Pointer to the next node in the list
} Appointment;

/* ======================= UI HELPER FUNCTIONS ======================= */

/* Prints a centered header with a cyan border */
static void print_header(const char* title) {
    set_color(COLOR_CYAN);
    printf("\n======================================================================\n");
    printf("  %s\n", title);
    printf("======================================================================\n");
    reset_color();
}

/* Prints a success message in green */
static void print_success(const char* msg) {
    set_color(COLOR_GREEN);
    printf("[OK] %s\n", msg);
    reset_color();
}

/* Prints an error message in red */
static void print_error(const char* msg) {
    set_color(COLOR_RED);
    printf("[ERR] %s\n", msg);
    reset_color();
}

/* Prints a warning message in yellow */
static void print_warning(const char* msg) {
    set_color(COLOR_YELLOW);
    printf("[WARN] %s\n", msg);
    reset_color();
}

/* ======================= UTILITY FUNCTIONS ======================= */

/* Removes the trailing newline character from a string */
static void trim_newline(char* s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
}

/* Clears the input buffer (used when input exceeds buffer size) */
static void flush_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* Safely reads a line of text from stdin */
static void read_line(const char* prompt, char* out, size_t cap) {
    if (!out || cap == 0) return;

    printf("%s", prompt);

    // Read input
    if (!fgets(out, (int)cap, stdin)) {
        out[0] = '\0';
        return;
    }

    // If buffer was too small, discard the rest of the line
    if (strchr(out, '\n') == NULL) flush_stdin();

    // Remove newline for clean string processing
    trim_newline(out);
}

/* Reads an integer from stdin with validation loop */
static int read_int(const char* prompt) {
    char buf[64];
    while (1) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        if (strchr(buf, '\n') == NULL) flush_stdin();
        trim_newline(buf);

        if (buf[0] == '\0') return -1;

        char* end = NULL;
        long v = strtol(buf, &end, 10);
        // Check if the whole string was a number
        if (end && *end == '\0') return (int)v;

        print_error("Invalid number. Try again.");
    }
}

/* Checks if a string contains the pipe character (used as delimiter in file) */
static int contains_pipe(const char* s) {
    return (s && strchr(s, '|') != NULL);
}

/* Validates date format (YYYY-MM-DD) and logical correctness */
static int validate_date(const char* date) {
    if (!date) return 0;
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;

    // Check digits
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!(date[i] >= '0' && date[i] <= '9')) return 0;
    }

    // Extract components
    int year  = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + (date[3]-'0');
    int month = (date[5]-'0')*10 + (date[6]-'0');
    int day   = (date[8]-'0')*10 + (date[9]-'0');

    // Basic range checks
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > 31) return 0;

    // Check days in month (30 days)
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) return 0;

    // February and leap year check
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if ((!leap && day > 28) || (leap && day > 29)) return 0;
    }

    return 1;
}

/* Compares two dates. Returns <0 if a<b, 0 if equal, >0 if a>b */
static int cmp_date(const char* a, const char* b) {
    return strcmp(a, b);
}

/* Checks if 'sub' exists inside 'text', case-insensitive */
static int contains_substring_ci(const char* text, const char* sub) {
    if (!text || !sub || sub[0] == '\0') return 0;

    for (size_t i = 0; text[i]; i++) {
        size_t j = 0;
        while (sub[j] && text[i + j] &&
               tolower((unsigned char)text[i + j]) == tolower((unsigned char)sub[j])) {
            j++;
        }
        if (sub[j] == '\0') return 1; // Match found
    }
    return 0;
}

/* Validates input fields to ensure they don't contain the file delimiter '|' */
static int validate_no_pipe_field(const char* field_name, const char* value) {
    if (contains_pipe(value)) {
        char msg[120];
        snprintf(msg, sizeof(msg), "Character '|' is not allowed in %s.", field_name);
        print_error(msg);
        return 0;
    }
    return 1;
}

/* ======================= LINKED LIST OPERATIONS ======================= */

/* Creates a new appointment node with dynamic memory allocation */
static Appointment* create_appointment(int id,
                                       const char* name,
                                       const char* phone,
                                       const char* date,
                                       const char* location,
                                       const char* subject) {
    Appointment* n = (Appointment*)malloc(sizeof(Appointment));
    if (!n) { perror("malloc"); exit(1); }

    n->id = id;
    snprintf(n->name,     sizeof(n->name),     "%s", name);
    snprintf(n->phone,    sizeof(n->phone),    "%s", phone);
    snprintf(n->date,     sizeof(n->date),     "%s", date);
    snprintf(n->location, sizeof(n->location), "%s", location);
    snprintf(n->subject,  sizeof(n->subject),  "%s", subject);
    n->next = NULL;
    return n;
}

/* Frees all nodes in the list to prevent memory leaks */
static void free_list(Appointment** head) {
    if (!head) return;
    Appointment* p = *head;
    while (p) {
        Appointment* nxt = p->next;
        free(p);
        p = nxt;
    }
    *head = NULL;
}

/* Checks if an ID already exists in the list */
static int is_id_unique(Appointment* head, int id) {
    for (Appointment* p = head; p; p = p->next)
        if (p->id == id) return 0;
    return 1;
}

/* Finds and returns an appointment node by its ID */
static Appointment* find_by_id(Appointment* head, int id) {
    for (Appointment* p = head; p; p = p->next)
        if (p->id == id) return p;
    return NULL;
}

/* Inserts a node into the list, sorted by Date (asc), then ID (asc) */
static void insert_sorted(Appointment** head, Appointment* node) {
    if (!head || !node) return;

    // Insert at head if list is empty or new node is smaller than head
    if (*head == NULL ||
        cmp_date(node->date, (*head)->date) < 0 ||
        (cmp_date(node->date, (*head)->date) == 0 && node->id < (*head)->id)) {
        node->next = *head;
        *head = node;
        return;
    }

    // Traverse to find correct position
    Appointment* cur = *head;
    while (cur->next &&
           (cmp_date(cur->next->date, node->date) < 0 ||
            (cmp_date(cur->next->date, node->date) == 0 && cur->next->id < node->id))) {
        cur = cur->next;
    }

    // Insert node
    node->next = cur->next;
    cur->next = node;
}

/* Removes a node by ID from the list but DOES NOT free it (returns detached node) */
static Appointment* detach_by_id(Appointment** head, int id) {
    if (!head || !*head) return NULL;

    Appointment* cur = *head;
    Appointment* prev = NULL;

    // Search for node
    while (cur && cur->id != id) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return NULL; // Not found

    // Unlink node
    if (!prev) *head = cur->next;
    else prev->next = cur->next;

    cur->next = NULL;
    return cur;
}

/* ======================= FILE OPERATIONS ======================= */

/* Writes the entire list to the text file */
static int save_to_file(Appointment* head) {
    FILE* f = fopen(FILE_NAME, "w");
    if (!f) return 0;

    for (Appointment* p = head; p; p = p->next) {
        fprintf(f, "%d|%s|%s|%s|%s|%s\n",
                p->id, p->name, p->phone, p->date, p->location, p->subject);
    }
    fclose(f);
    return 1;
}

/* Loads appointments from file into the list at startup */
static void load_from_file(Appointment** head) {
    FILE* f = fopen(FILE_NAME, "r");
    if (!f) return; // File doesn't exist yet

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);

        // Parse pipe-delimited fields
        char* id_str   = strtok(line, "|");
        char* name     = strtok(NULL, "|");
        char* phone    = strtok(NULL, "|");
        char* date     = strtok(NULL, "|");
        char* location = strtok(NULL, "|");
        char* subject  = strtok(NULL, "|");

        // Skip malformed lines
        if (!id_str || !name || !phone || !date || !location || !subject) continue;

        int id = atoi(id_str);
        if (id <= 0) continue;
        if (!validate_date(date)) continue;

        // Prevent duplicates if file was manually edited
        if (!is_id_unique(*head, id)) continue;

        Appointment* node = create_appointment(id, name, phone, date, location, subject);
        insert_sorted(head, node);
    }

    fclose(f);
}

/* ======================= DISPLAY FUNCTIONS ======================= */

/* Displays a single appointment details in a block format */
static void display_one(const Appointment* p) {
    if (!p) return;

    printf("--------------------------------------------------\n");
    printf("ID       : %d\n", p->id);
    printf("Name     : %s\n", p->name);
    printf("Phone    : %s\n", p->phone);
    printf("Date     : %s\n", p->date);
    printf("Location : %s\n", p->location);
    printf("Subject  : %s\n", p->subject);
    printf("--------------------------------------------------\n");
}

/* Displays all appointments in a table format */
static void display_all(Appointment* head) {
    if (!head) {
        print_warning("No appointments found.");
        return;
    }

    print_header("AppointC - ALL APPOINTMENTS");

    // Table Header
    printf("%-5s %-20s %-15s %-12s %-20s %-20s\n",
           "ID", "Name", "Phone", "Date", "Location", "Subject");
    printf("--------------------------------------------------------------------------------\n");

    // Table Rows
    for (Appointment* p = head; p; p = p->next) {
        printf("%-5d %-20s %-15s %-12s %-20s %-20s\n",
               p->id, p->name, p->phone, p->date, p->location, p->subject);
    }
}

/* ======================= ACTION FUNCTIONS ======================= */

/* Logic for adding a new appointment */
static void add_appointment(Appointment** head) {
    if (!head) return;

    print_header("AppointC - ADD NEW APPOINTMENT");

    // Get and validate ID
    int id = read_int("ID : ");
    if (id <= 0) { print_error("Invalid ID."); return; }
    if (!is_id_unique(*head, id)) { print_error("ID already exists."); return; }

    char name[MAX_TEXT], phone[MAX_TEXT], date[DATE_LEN + 1], location[MAX_TEXT], subject[MAX_TEXT];

    // Get inputs
    read_line("Name     : ", name, sizeof(name));
    if (name[0] == '\0') { print_error("Name cannot be empty."); return; }
    if (!validate_no_pipe_field("Name", name)) return;

    read_line("Phone    : ", phone, sizeof(phone));
    if (!validate_no_pipe_field("Phone", phone)) return;

    read_line("Date (YYYY-MM-DD) : ", date, sizeof(date));
    if (!validate_date(date)) { print_error("Invalid date."); return; }

    read_line("Location : ", location, sizeof(location));
    if (!validate_no_pipe_field("Location", location)) return;

    read_line("Subject  : ", subject, sizeof(subject));
    if (!validate_no_pipe_field("Subject", subject)) return;

    // Create and insert
    Appointment* node = create_appointment(id, name, phone, date, location, subject);
    insert_sorted(head, node);

    if (!save_to_file(*head)) print_error("Failed to save file.");
    else print_success("Appointment added successfully.");
}

/* Logic for searching appointments */
static void search_appointment(Appointment* head) {
    print_header("AppointC - SEARCH");

    int choice = read_int("1) By ID\n2) By name (partial)\n3) By date\nChoice : ");
    if (choice == 1) {
        int id = read_int("ID : ");
        Appointment* p = find_by_id(head, id);
        if (!p) { print_error("Not found."); return; }
        display_one(p);

    } else if (choice == 2) {
        char q[MAX_TEXT];
        read_line("Name (or part) : ", q, sizeof(q));
        if (q[0] == '\0') { print_warning("Empty search query."); return; }

        int found = 0;
        for (Appointment* p = head; p; p = p->next) {
            if (contains_substring_ci(p->name, q)) {
                display_one(p);
                found = 1;
            }
        }
        if (!found) print_warning("No appointments found.");

    } else if (choice == 3) {
        char d[DATE_LEN + 1];
        read_line("Date (YYYY-MM-DD) : ", d, sizeof(d));
        if (!validate_date(d)) { print_error("Invalid date format."); return; }

        int found = 0;
        for (Appointment* p = head; p; p = p->next) {
            if (strcmp(p->date, d) == 0) {
                display_one(p);
                found = 1;
            }
        }
        if (!found) print_warning("No appointments found.");
    } else {
        print_error("Invalid choice.");
    }
}

/* Logic for displaying schedule between two dates */
static void display_schedule(Appointment* head) {
    print_header("AppointC - SCHEDULE (BETWEEN TWO DATES)");

    char start[DATE_LEN + 1], end[DATE_LEN + 1];
    read_line("Start date (YYYY-MM-DD) : ", start, sizeof(start));
    if (!validate_date(start)) { print_error("Invalid start date."); return; }

    read_line("End date   (YYYY-MM-DD) : ", end, sizeof(end));
    if (!validate_date(end)) { print_error("Invalid end date."); return; }

    if (cmp_date(start, end) > 0) {
        print_error("Start date cannot be after end date.");
        return;
    }

    printf("\nSCHEDULE FROM %s TO %s\n", start, end);
    printf("%-5s %-20s %-12s %-20s %-20s\n", "ID", "Name", "Date", "Location", "Subject");
    printf("--------------------------------------------------------------------------------\n");

    int found = 0;
    for (Appointment* p = head; p; p = p->next) {
        if (cmp_date(p->date, start) >= 0 && cmp_date(p->date, end) <= 0) {
            printf("%-5d %-20s %-12s %-20s %-20s\n",
                   p->id, p->name, p->date, p->location, p->subject);
            found = 1;
        }
    }

    if (!found) print_warning("No appointments found in this period.");
}

/* Logic for modifying an existing appointment */
static void modify_appointment(Appointment** head) {
    if (!head || !*head) { print_warning("No appointments."); return; }

    print_header("AppointC - MODIFY");

    // Detach the node to edit it
    int id = read_int("ID to modify : ");
    Appointment* node = detach_by_id(head, id);
    if (!node) { print_error("ID not found."); return; }

    print_warning("Current appointment:");
    display_one(node);

    char buf[128];

    // Update Date
    read_line("New date (ENTER to keep) : ", buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (!validate_date(buf)) {
            print_error("Invalid date. Keeping old value.");
        } else {
            snprintf(node->date, sizeof(node->date), "%s", buf);
        }
    }

    // Update Location
    read_line("New location (ENTER to keep) : ", buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (!validate_no_pipe_field("Location", buf)) {
            insert_sorted(head, node); // Re-insert before returning
            return;
        }
        snprintf(node->location, sizeof(node->location), "%s", buf);
    }

    // Update Subject
    read_line("New subject (ENTER to keep) : ", buf, sizeof(buf));
    if (buf[0] != '\0') {
        if (!validate_no_pipe_field("Subject", buf)) {
            insert_sorted(head, node); // Re-insert before returning
            return;
        }
        snprintf(node->subject, sizeof(node->subject), "%s", buf);
    }

    // Re-insert into list (position might change if date changed)
    insert_sorted(head, node);

    if (!save_to_file(*head)) print_error("Failed to save file.");
    else print_success("Appointment modified successfully.");
}

/* Deletes a specific appointment by ID */
static void delete_by_id(Appointment** head) {
    if (!head || !*head) { print_warning("No appointments."); return; }

    print_header("AppointC - DELETE BY ID");

    int id = read_int("ID to delete : ");
    Appointment* node = detach_by_id(head, id);
    if (!node) { print_error("Not found."); return; }

    print_warning("Appointment to delete:");
    display_one(node);

    char rep[16];
    read_line("Confirm deletion (y/n) : ", rep, sizeof(rep));
    if (!(rep[0] == 'y' || rep[0] == 'Y')) {
        insert_sorted(head, node); // Cancel: put it back
        print_warning("Deletion cancelled.");
        return;
    }

    free(node); // Confirm: free memory

    if (!save_to_file(*head)) print_error("Failed to save file.");
    else print_success("Appointment deleted.");
}

/* Deletes all appointments matching a specific date */
static void delete_by_date(Appointment** head) {
    if (!head || !*head) { print_warning("No appointments."); return; }

    print_header("AppointC - DELETE BY DATE");

    char date[DATE_LEN + 1];
    read_line("Date (YYYY-MM-DD) : ", date, sizeof(date));
    if (!validate_date(date)) { print_error("Invalid date."); return; }

    // Count matches first for user confirmation
    int cnt = 0;
    for (Appointment* p = *head; p; p = p->next)
        if (strcmp(p->date, date) == 0) cnt++;

    if (cnt == 0) { print_warning("No appointments found for this date."); return; }

    char rep[16];
    char msg[120];
    snprintf(msg, sizeof(msg), "%d appointment(s) will be deleted.", cnt);
    print_warning(msg);

    read_line("Confirm deletion (y/n) : ", rep, sizeof(rep));
    if (!(rep[0] == 'y' || rep[0] == 'Y')) {
        print_warning("Deletion cancelled.");
        return;
    }

    // Perform deletion
    Appointment* cur = *head;
    Appointment* prev = NULL;

    while (cur) {
        if (strcmp(cur->date, date) == 0) {
            Appointment* toDel = cur;
            cur = cur->next;
            if (!prev) *head = cur; // Deleting head
            else prev->next = cur;  // Deleting middle/end
            free(toDel);
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    if (!save_to_file(*head)) print_error("Failed to save file.");
    else print_success("Appointments deleted for that date.");
}

/* Deletes all appointments from the list */
static void delete_all(Appointment** head) {
    if (!head || !*head) { print_warning("No appointments."); return; }

    print_header("AppointC - DELETE ALL");

    char rep[16];
    print_warning("This will delete ALL appointments.");
    read_line("Confirm deletion (y/n) : ", rep, sizeof(rep));
    if (!(rep[0] == 'y' || rep[0] == 'Y')) {
        print_warning("Deletion cancelled.");
        return;
    }

    free_list(head);

    if (!save_to_file(*head)) print_error("Failed to save file.");
    else print_success("All appointments deleted.");
}

/* Menu logic for delete options */
static void delete_appointment(Appointment** head) {
    print_header("AppointC - DELETE");

    int choice = read_int("1) Delete by ID\n2) Delete by date\n3) Delete all\nChoice : ");
    if (choice == 1) delete_by_id(head);
    else if (choice == 2) delete_by_date(head);
    else if (choice == 3) delete_all(head);
    else print_error("Invalid choice.");
}

/* ======================= MENU DISPLAY ======================= */

/* Main menu user interface */
static void display_menu(void) {
    printf("\n+-------------------------------------------------------------+\n");
    printf("|                         AppointC                            |\n");
    printf("|                 Appointment Manager (C)                     |\n");
    printf("+-------------------------------------------------------------+\n");
    printf(" 1) Add an appointment\n");
    printf(" 2) Display all appointments\n");
    printf(" 3) Delete an appointment\n");
    printf(" 4) Search appointments\n");
    printf(" 5) Display schedule\n");
    printf(" 6) Modify an appointment\n");
    printf(" 0) Quit\n");
}

/* ======================= MAIN ENTRY POINT ======================= */
int main(void) {
    console_init(); // Setup colors

    Appointment* head = NULL;
    load_from_file(&head); // Load existing data

    print_success("AppointC started. Data loaded from rdv.txt (if available).");

    while (1) {
        display_menu();
        int choice = read_int("Your choice : ");

        switch (choice) {
            case 1: add_appointment(&head); break;
            case 2: display_all(head); break;
            case 3: delete_appointment(&head); break;
            case 4: search_appointment(head); break;
            case 5: display_schedule(head); break;
            case 6: modify_appointment(&head); break;
            case 0:
                save_to_file(head);
                free_list(&head);
                print_success("Goodbye. AppointC closed.");
                return 0;
            default:
                print_error("Invalid choice.");
        }
    }
}
