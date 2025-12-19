//Emam Samara
//1220022
//section 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME_LEN 128
#define MAX_ID_LEN 32
#define MAX_MAJOR_LEN 128
#define MAX_CODE_LEN 32
#define MAX_TITLE_LEN 128
#define MAX_SEMESTER_LEN 64
#define LINE_BUFFER 512

typedef struct CourseRecord {
    char courseCode[MAX_CODE_LEN];
    char courseTitle[MAX_TITLE_LEN];
    int creditHours;
    char semester[MAX_SEMESTER_LEN];
    struct CourseRecord* next;
} CourseRecord;

typedef struct StudentRecord {
    char name[MAX_NAME_LEN];
    char studentID[MAX_ID_LEN];
    char major[MAX_MAJOR_LEN];
    CourseRecord* courses;
} StudentRecord;

typedef struct AVLNode {
    StudentRecord data;
    struct AVLNode* left;
    struct AVLNode* right;
    int height;
} AVLNode;

typedef enum {
    HASH_EMPTY,
    HASH_OCCUPIED,
    HASH_DELETED
} SlotState;

typedef struct {
    SlotState state;
    char name[MAX_NAME_LEN];
    AVLNode* reference;
} HashEntry;

typedef struct {
    HashEntry* entries;
    int capacity;
    int size;
} HashTable;

typedef struct {
    AVLNode** data;
    int size;
    int capacity;
} NodeList;

static void trim(char* s) {
    if (!s) return;
    size_t start = 0;
    size_t len = strlen(s);
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    if (start > 0) memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

static void readLine(char* buffer, size_t size) {
    if (!fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
    trim(buffer);
}

static int isLettersSpaces(const char* s) {
    if (!s || *s == '\0') return 0;
    int hasLetter = 0;
    for (size_t i = 0; s[i]; ++i) {
        if (isalpha((unsigned char)s[i])) {
            hasLetter = 1;
            continue;
        }
        if (s[i] == ' ') continue;
        return 0;
    }
    return hasLetter;
}

static int isDigitsOnly(const char* s) {
    if (!s || *s == '\0') return 0;
    for (size_t i = 0; s[i]; ++i) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

static int isCourseCode(const char* s) {
    if (!s || *s == '\0') return 0;
    for (size_t i = 0; s[i]; ++i) {
        if (!isalnum((unsigned char)s[i])) return 0;
    }
    return 1;
}

static int isSemesterString(const char* s) {
    if (!s || *s == '\0') return 0;
    for (size_t i = 0; s[i]; ++i) {
        if (!isalnum((unsigned char)s[i]) && s[i] != ' ') return 0;
    }
    return 1;
}

typedef int (*ValidatorFn)(const char*);

static void getValidatedInput(const char* prompt, char* buffer, size_t size, ValidatorFn validator, const char* errorMsg) {
    while (1) {
        if (prompt) printf("%s", prompt);
        readLine(buffer, size);
        if (validator(buffer)) break;
        printf("%s", errorMsg);
    }
}

static int equalsIgnoreCase(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int readIntWithPrompt(const char* prompt, int minValue, int maxValue) {
    char buffer[64];
    for (;;) {
        if (prompt) printf("%s", prompt);
        if (!fgets(buffer, sizeof(buffer), stdin)) return minValue;
        char* endptr = NULL;
        long value = strtol(buffer, &endptr, 10);
        if (endptr == buffer) {
            printf("[ERROR] Invalid number. Try again.\n");
            continue;
        }
        while (*endptr) {
            if (!isspace((unsigned char)*endptr)) break;
            endptr++;
        }
        if (*endptr && *endptr != '\n') {
            printf("[ERROR] Invalid characters detected. Try again.\n");
            continue;
        }
        if (value < minValue || value > maxValue) {
            printf("[ERROR] Value must be between %d and %d.\n", minValue, maxValue);
            continue;
        }
        return (int)value;
    }
}

static CourseRecord* createCourseRecord(const char* code, const char* title, int hours, const char* semester) {
    CourseRecord* record = (CourseRecord*)malloc(sizeof(CourseRecord));
    if (!record) return NULL;
    strncpy(record->courseCode, code, MAX_CODE_LEN - 1);
    record->courseCode[MAX_CODE_LEN - 1] = '\0';
    strncpy(record->courseTitle, title, MAX_TITLE_LEN - 1);
    record->courseTitle[MAX_TITLE_LEN - 1] = '\0';
    record->creditHours = hours;
    strncpy(record->semester, semester, MAX_SEMESTER_LEN - 1);
    record->semester[MAX_SEMESTER_LEN - 1] = '\0';
    record->next = NULL;
    return record;
}

static void freeCourseRecords(CourseRecord* head) {
    while (head) {
        CourseRecord* temp = head;
        head = head->next;
        free(temp);
    }
}

static CourseRecord* cloneCourseRecords(const CourseRecord* head) {
    CourseRecord* newHead = NULL;
    CourseRecord* tail = NULL;
    while (head) {
        CourseRecord* node = createCourseRecord(head->courseCode, head->courseTitle, head->creditHours, head->semester);
        if (!node) break;
        if (!newHead) newHead = node;
        else tail->next = node;
        tail = node;
        head = head->next;
    }
    return newHead;
}

static int appendCourseRecord(CourseRecord** head, CourseRecord* record) {
    if (!record) return 0;
    CourseRecord* current = *head;
    while (current) {
        if (equalsIgnoreCase(current->courseCode, record->courseCode)) {
            strncpy(current->courseTitle, record->courseTitle, MAX_TITLE_LEN - 1);
            current->courseTitle[MAX_TITLE_LEN - 1] = '\0';
            current->creditHours = record->creditHours;
            strncpy(current->semester, record->semester, MAX_SEMESTER_LEN - 1);
            current->semester[MAX_SEMESTER_LEN - 1] = '\0';
            free(record);
            return 0;
        }
        current = current->next;
    }
    if (!*head) {
        *head = record;
    } else {
        CourseRecord* tail = *head;
        while (tail->next) tail = tail->next;
        tail->next = record;
    }
    return 1;
}

static int removeCourseRecord(CourseRecord** head, const char* courseCode) {
    CourseRecord* current = *head;
    CourseRecord* prev = NULL;
    while (current) {
        if (equalsIgnoreCase(current->courseCode, courseCode)) {
            if (prev) prev->next = current->next;
            else *head = current->next;
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

static int hasCourse(const CourseRecord* head, const char* courseCode) {
    while (head) {
        if (equalsIgnoreCase(head->courseCode, courseCode)) return 1;
        head = head->next;
    }
    return 0;
}

static int nodeHeight(AVLNode* node) {
    return node ? node->height : 0;
}

static int maxInt(int a, int b) {
    return a > b ? a : b;
}

static AVLNode* createNode(const char* name, const char* id, const char* major, CourseRecord* course) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    if (!node) return NULL;
    strncpy(node->data.name, name, MAX_NAME_LEN - 1);
    node->data.name[MAX_NAME_LEN - 1] = '\0';
    strncpy(node->data.studentID, id, MAX_ID_LEN - 1);
    node->data.studentID[MAX_ID_LEN - 1] = '\0';
    strncpy(node->data.major, major, MAX_MAJOR_LEN - 1);
    node->data.major[MAX_MAJOR_LEN - 1] = '\0';
    node->data.courses = NULL;
    appendCourseRecord(&node->data.courses, course);
    node->left = node->right = NULL;
    node->height = 1;
    return node;
}

static void copyStudentRecord(StudentRecord* dest, const StudentRecord* src) {
    strncpy(dest->name, src->name, MAX_NAME_LEN - 1);
    dest->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(dest->studentID, src->studentID, MAX_ID_LEN - 1);
    dest->studentID[MAX_ID_LEN - 1] = '\0';
    strncpy(dest->major, src->major, MAX_MAJOR_LEN - 1);
    dest->major[MAX_MAJOR_LEN - 1] = '\0';
    freeCourseRecords(dest->courses);
    dest->courses = cloneCourseRecords(src->courses);
}

static AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = maxInt(nodeHeight(y->left), nodeHeight(y->right)) + 1;
    x->height = maxInt(nodeHeight(x->left), nodeHeight(x->right)) + 1;
    return x;
}

static AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = maxInt(nodeHeight(x->left), nodeHeight(x->right)) + 1;
    y->height = maxInt(nodeHeight(y->left), nodeHeight(y->right)) + 1;
    return y;
}

static int getBalance(AVLNode* node) {
    if (!node) return 0;
    return nodeHeight(node->left) - nodeHeight(node->right);
}

static AVLNode* insertRegistration(AVLNode* node, const char* name, const char* id, const char* major, CourseRecord* course, int* newStudent, int* newCourse) {
    if (!node) {
        if (newStudent) *newStudent = 1;
        if (newCourse) *newCourse = 1;
        return createNode(name, id, major, course);
    }
    int cmp = strcmp(id, node->data.studentID);
    if (cmp < 0) {
        node->left = insertRegistration(node->left, name, id, major, course, newStudent, newCourse);
    } else if (cmp > 0) {
        node->right = insertRegistration(node->right, name, id, major, course, newStudent, newCourse);
    } else {
        if (newStudent) *newStudent = 0;
        strncpy(node->data.name, name, MAX_NAME_LEN - 1);
        node->data.name[MAX_NAME_LEN - 1] = '\0';
        strncpy(node->data.major, major, MAX_MAJOR_LEN - 1);
        node->data.major[MAX_MAJOR_LEN - 1] = '\0';
        int added = appendCourseRecord(&node->data.courses, course);
        if (newCourse) *newCourse = added;
        if (!added) return node;
    }
    node->height = 1 + maxInt(nodeHeight(node->left), nodeHeight(node->right));
    int balance = getBalance(node);
    if (balance > 1 && strcmp(id, node->left->data.studentID) < 0) return rightRotate(node);
    if (balance < -1 && strcmp(id, node->right->data.studentID) > 0) return leftRotate(node);
    if (balance > 1 && strcmp(id, node->left->data.studentID) > 0) {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }
    if (balance < -1 && strcmp(id, node->right->data.studentID) < 0) {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }
    return node;
}

static AVLNode* minValueNode(AVLNode* node) {
    AVLNode* current = node;
    while (current && current->left) current = current->left;
    return current;
}

static AVLNode* deleteStudent(AVLNode* root, const char* studentID) {
    if (!root) return NULL;
    int cmp = strcmp(studentID, root->data.studentID);
    if (cmp < 0) root->left = deleteStudent(root->left, studentID);
    else if (cmp > 0) root->right = deleteStudent(root->right, studentID);
    else {
        if (!root->left || !root->right) {
            AVLNode* temp = root->left ? root->left : root->right;
            if (!temp) {
                freeCourseRecords(root->data.courses);
                free(root);
                return NULL;
            } else {
                AVLNode tempNode = *temp;
                freeCourseRecords(root->data.courses);
                *root = tempNode;
                free(temp);
            }
        } else {
            AVLNode* temp = minValueNode(root->right);
            copyStudentRecord(&root->data, &temp->data);
            root->right = deleteStudent(root->right, temp->data.studentID);
        }
    }
    root->height = 1 + maxInt(nodeHeight(root->left), nodeHeight(root->right));
    int balance = getBalance(root);
    if (balance > 1 && getBalance(root->left) >= 0) return rightRotate(root);
    if (balance > 1 && getBalance(root->left) < 0) {
        root->left = leftRotate(root->left);
        return rightRotate(root);
    }
    if (balance < -1 && getBalance(root->right) <= 0) return leftRotate(root);
    if (balance < -1 && getBalance(root->right) > 0) {
        root->right = rightRotate(root->right);
        return leftRotate(root);
    }
    return root;
}

static AVLNode* findByID(AVLNode* root, const char* studentID) {
    while (root) {
        int cmp = strcmp(studentID, root->data.studentID);
        if (cmp == 0) return root;
        root = cmp < 0 ? root->left : root->right;
    }
    return NULL;
}

static void initNodeList(NodeList* list) {
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

static void appendNode(NodeList* list, AVLNode* node) {
    if (list->size == list->capacity) {
        int newCap = list->capacity == 0 ? 4 : list->capacity * 2;
        AVLNode** resized = (AVLNode**)realloc(list->data, newCap * sizeof(AVLNode*));
        if (!resized) return;
        list->data = resized;
        list->capacity = newCap;
    }
    list->data[list->size++] = node;
}

static void collectByName(AVLNode* root, const char* name, NodeList* list) {
    if (!root) return;
    collectByName(root->left, name, list);
    if (equalsIgnoreCase(root->data.name, name)) appendNode(list, root);
    collectByName(root->right, name, list);
}

static void freeNodeList(NodeList* list) {
    free(list->data);
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

static void printCourses(const CourseRecord* head) {
    int index = 1;
    while (head) {
        printf("    [%d] %s | %s | %d CH | %s\n", index, head->courseCode, head->courseTitle, head->creditHours, head->semester);
        head = head->next;
        index++;
    }
}

static void printStudent(const AVLNode* node) {
    if (!node) return;
    printf("Name: %s\n", node->data.name);
    printf("Student ID: %s\n", node->data.studentID);
    printf("Major: %s\n", node->data.major);
    printf("Courses:\n");
    if (!node->data.courses) printf("    None\n");
    else printCourses(node->data.courses);
}

static void listStudentsByCourse(AVLNode* root, const char* courseCode, int* count) {
    if (!root) return;
    listStudentsByCourse(root->left, courseCode, count);
    if (hasCourse(root->data.courses, courseCode)) {
        printStudent(root);
        (*count)++;
    }
    listStudentsByCourse(root->right, courseCode, count);
}

static void saveNodeCourses(FILE* file, const AVLNode* node) {
    const CourseRecord* course = node->data.courses;
    while (course) {
        fprintf(file, "%s#%s#%s#%s#%s#%d#%s\n", node->data.name, node->data.studentID, node->data.major, course->courseCode, course->courseTitle, course->creditHours, course->semester);
        course = course->next;
    }
}

static void saveTreeRecursive(FILE* file, AVLNode* node) {
    if (!node) return;
    saveTreeRecursive(file, node->left);
    saveNodeCourses(file, node);
    saveTreeRecursive(file, node->right);
}

static void saveTreeToFile(const char* filename, AVLNode* root) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("[ERROR] Unable to write to %s\n", filename);
        return;
    }
    saveTreeRecursive(file, root);
    fclose(file);
    printf("[OK] Data saved to %s\n", filename);
}

static void traverseAndInsertToList(AVLNode* root, AVLNode** array, int* index) {
    if (!root) return;
    traverseAndInsertToList(root->left, array, index);
    array[(*index)++] = root;
    traverseAndInsertToList(root->right, array, index);
}

static int countStudents(AVLNode* root) {
    if (!root) return 0;
    return 1 + countStudents(root->left) + countStudents(root->right);
}

static void exportStudentsRecursive(FILE* file, AVLNode* node) {
    if (!node) return;
    exportStudentsRecursive(file, node->left);
    fprintf(file, "%s#%s#%s\n", node->data.name, node->data.studentID, node->data.major);
    exportStudentsRecursive(file, node->right);
}

static void exportStudentsData(const char* filename, AVLNode* root) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("[ERROR] Cannot open %s\n", filename);
        return;
    }
    exportStudentsRecursive(file, root);
    fclose(file);
    printf("[OK] Student data exported to %s\n", filename);
}

static AVLNode* loadFromFile(const char* filename, AVLNode* root) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("[INFO] File %s not found. Starting with empty records.\n", filename);
        return root;
    }
    char line[LINE_BUFFER];
    int loaded = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        if (strlen(line) == 0) continue;
        char* tokens[7];
        int count = 0;
        char* token = strtok(line, "#");
        while (token && count < 7) {
            tokens[count++] = token;
            token = strtok(NULL, "#");
        }
        if (count != 7) continue;
        char* name = tokens[0];
        char* id = tokens[1];
        char* major = tokens[2];
        char* courseCode = tokens[3];
        char* courseTitle = tokens[4];
        int hours = atoi(tokens[5]);
        char* semester = tokens[6];
        CourseRecord* course = createCourseRecord(courseCode, courseTitle, hours, semester);
        if (!course) continue;
        root = insertRegistration(root, name, id, major, course, NULL, NULL);
        loaded++;
    }
    fclose(file);
    printf("[OK] Loaded %d registrations from %s\n", loaded, filename);
    return root;
}

static void freeTree(AVLNode* root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    freeCourseRecords(root->data.courses);
    free(root);
}

static int isPrime(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

static int nextPrime(int n) {
    while (!isPrime(n)) n++;
    return n;
}

static void initHashTable(HashTable* table, int capacity) {
    table->capacity = capacity;
    table->size = 0;
    table->entries = (HashEntry*)malloc(sizeof(HashEntry) * capacity);
    for (int i = 0; i < capacity; i++) {
        table->entries[i].state = HASH_EMPTY;
        table->entries[i].name[0] = '\0';
        table->entries[i].reference = NULL;
    }
}

static void freeHashTable(HashTable* table) {
    free(table->entries);
    table->entries = NULL;
    table->capacity = 0;
    table->size = 0;
}

static unsigned long hashString(const char* s, int mod) {
    unsigned long hash = 0;
    while (*s) {
        hash = (hash * 131) + (unsigned char)tolower((unsigned char)*s);
        s++;
    }
    return hash % (unsigned long)mod;
}

static int hashInsert(HashTable* table, const char* name, AVLNode* reference) {
    if (!table->entries) return 0;
    if (table->size >= table->capacity) return 0;
    unsigned long index = hashString(name, table->capacity);
    int firstDeleted = -1;
    for (int i = 0; i < table->capacity; i++) {
        unsigned long probe = (index + i) % table->capacity;
        HashEntry* entry = &table->entries[probe];
        if (entry->state == HASH_EMPTY) {
            int target = firstDeleted >= 0 ? firstDeleted : (int)probe;
            entry = &table->entries[target];
            strncpy(entry->name, name, MAX_NAME_LEN - 1);
            entry->name[MAX_NAME_LEN - 1] = '\0';
            entry->reference = reference;
            entry->state = HASH_OCCUPIED;
            table->size++;
            return 1;
        }
        if (entry->state == HASH_DELETED && firstDeleted < 0) firstDeleted = (int)probe;
        else if (entry->state == HASH_OCCUPIED && equalsIgnoreCase(entry->name, name)) {
            entry->reference = reference;
            strncpy(entry->name, name, MAX_NAME_LEN - 1);
            entry->name[MAX_NAME_LEN - 1] = '\0';
            return 1;
        }
    }
    if (firstDeleted >= 0) {
        HashEntry* entry = &table->entries[firstDeleted];
        strncpy(entry->name, name, MAX_NAME_LEN - 1);
        entry->name[MAX_NAME_LEN - 1] = '\0';
        entry->reference = reference;
        entry->state = HASH_OCCUPIED;
        table->size++;
        return 1;
    }
    return 0;
}

static int hashSearch(HashTable* table, const char* name) {
    if (!table->entries) return -1;
    unsigned long index = hashString(name, table->capacity);
    for (int i = 0; i < table->capacity; i++) {
        unsigned long probe = (index + i) % table->capacity;
        HashEntry* entry = &table->entries[probe];
        if (entry->state == HASH_EMPTY) return -1;
        if (entry->state == HASH_OCCUPIED && equalsIgnoreCase(entry->name, name)) return (int)probe;
    }
    return -1;
}

static int hashDelete(HashTable* table, const char* name) {
    int index = hashSearch(table, name);
    if (index < 0) return 0;
    table->entries[index].state = HASH_DELETED;
    table->entries[index].reference = NULL;
    table->entries[index].name[0] = '\0';
    table->size--;
    return 1;
}

static void printHashTable(HashTable* table) {
    if (!table->entries) {
        printf("[INFO] Hash table is empty.\n");
        return;
    }
    for (int i = 0; i < table->capacity; i++) {
        HashEntry* entry = &table->entries[i];
        if (entry->state == HASH_OCCUPIED) {
            printf("%d: %s (ID %s)\n", i, entry->name, entry->reference ? entry->reference->data.studentID : "N/A");
        } else if (entry->state == HASH_DELETED) {
            printf("%d: <DELETED>\n", i);
        } else {
            printf("%d: <EMPTY>\n", i);
        }
    }
}

static void buildHashTable(HashTable* table, AVLNode* root) {
    int studentCount = countStudents(root);
    if (studentCount == 0) {
        FILE* empty = fopen("students_hash.data", "w");
        if (empty) fclose(empty);
        printf("[INFO] No student records to hash.\n");
        return;
    }
    if (table->entries) freeHashTable(table);
    int capacity = nextPrime(studentCount * 2 + 1);
    initHashTable(table, capacity);
    AVLNode** nodes = (AVLNode**)malloc(sizeof(AVLNode*) * studentCount);
    int index = 0;
    traverseAndInsertToList(root, nodes, &index);
    for (int i = 0; i < index; i++) hashInsert(table, nodes[i]->data.name, nodes[i]);
    free(nodes);
    exportStudentsData("students_hash.data", root);
    printf("[OK] Hash table built with %d slots.\n", capacity);
}

static void saveHashTableToFile(HashTable* table, const char* filename) {
    if (!table->entries) {
        printf("[ERROR] Hash table is not initialized.\n");
        return;
    }
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("[ERROR] Cannot open %s.\n", filename);
        return;
    }
    for (int i = 0; i < table->capacity; i++) {
        HashEntry* entry = &table->entries[i];
        if (entry->state == HASH_OCCUPIED && entry->reference) saveNodeCourses(file, entry->reference);
    }
    fclose(file);
    printf("[OK] Hash table data saved to %s\n", filename);
}

static void insertRegistrationFlow(AVLNode** root) {
    char name[MAX_NAME_LEN];
    char id[MAX_ID_LEN];
    char major[MAX_MAJOR_LEN];
    char courseCode[MAX_CODE_LEN];
    char courseTitle[MAX_TITLE_LEN];
    char semester[MAX_SEMESTER_LEN];
    getValidatedInput("Enter Student Name: ", name, sizeof(name), isLettersSpaces, "[ERROR] Name must contain letters only.\n");
    getValidatedInput("Enter Student ID: ", id, sizeof(id), isDigitsOnly, "[ERROR] Student ID must contain digits only.\n");
    getValidatedInput("Enter Major: ", major, sizeof(major), isLettersSpaces, "[ERROR] Major must contain letters only.\n");
    getValidatedInput("Enter Course Code: ", courseCode, sizeof(courseCode), isCourseCode, "[ERROR] Course code must be alphanumeric with no spaces.\n");
    getValidatedInput("Enter Course Title: ", courseTitle, sizeof(courseTitle), isLettersSpaces, "[ERROR] Course title must contain letters only.\n");
    int hours = readIntWithPrompt("Enter Credit Hours: ", 0, 20);
    getValidatedInput("Enter Semester: ", semester, sizeof(semester), isSemesterString, "[ERROR] Semester must contain letters and numbers only.\n");
    CourseRecord* course = createCourseRecord(courseCode, courseTitle, hours, semester);
    if (!course) {
        printf("[ERROR] Unable to create course record.\n");
        return;
    }
    int newStudent = 0;
    int newCourse = 0;
    *root = insertRegistration(*root, name, id, major, course, &newStudent, &newCourse);
    if (newStudent) printf("[OK] Student created with first registration.\n");
    else if (newCourse) printf("[OK] Added new course to existing student.\n");
    else printf("[INFO] Existing course updated.\n");
}

static void findStudentByNameFlow(AVLNode* root) {
    if (!root) {
        printf("[INFO] No student data available.\n");
        return;
    }
    char query[MAX_NAME_LEN];
    getValidatedInput("Enter Student Name: ", query, sizeof(query), isLettersSpaces, "[ERROR] Name must contain letters only.\n");
    NodeList list;
    initNodeList(&list);
    collectByName(root, query, &list);
    if (list.size == 0) {
        printf("[INFO] No student found with that name.\n");
        freeNodeList(&list);
        return;
    }
    AVLNode* target = NULL;
    if (list.size == 1) target = list.data[0];
    else {
        printf("Multiple students found:\n");
        for (int i = 0; i < list.size; i++) {
            printf("%d) %s (ID %s)\n", i + 1, list.data[i]->data.name, list.data[i]->data.studentID);
        }
        int choice = readIntWithPrompt("Select student number: ", 1, list.size);
        target = list.data[choice - 1];
    }
    printStudent(target);
    printf("Update this student's information? (1 = Yes, 2 = No): ");
    int update = readIntWithPrompt("", 1, 2);
    if (update == 1) {
        char buffer[MAX_MAJOR_LEN];
        while (1) {
            printf("Enter new name (leave blank to keep current): ");
            readLine(buffer, sizeof(buffer));
            if (strlen(buffer) == 0) break;
            if (isLettersSpaces(buffer)) {
                strncpy(target->data.name, buffer, MAX_NAME_LEN - 1);
                target->data.name[MAX_NAME_LEN - 1] = '\0';
                break;
            }
            printf("[ERROR] Name must contain letters only.\n");
        }
        while (1) {
            printf("Enter new major (leave blank to keep current): ");
            readLine(buffer, sizeof(buffer));
            if (strlen(buffer) == 0) break;
            if (isLettersSpaces(buffer)) {
                strncpy(target->data.major, buffer, MAX_MAJOR_LEN - 1);
                target->data.major[MAX_MAJOR_LEN - 1] = '\0';
                break;
            }
            printf("[ERROR] Major must contain letters only.\n");
        }
        printf("[OK] Student record updated.\n");
    }
    freeNodeList(&list);
}

static void listCourseFlow(AVLNode* root) {
    if (!root) {
        printf("[INFO] No student data available.\n");
        return;
    }
    char courseCode[MAX_CODE_LEN];
    getValidatedInput("Enter Course Code: ", courseCode, sizeof(courseCode), isCourseCode, "[ERROR] Course code must be alphanumeric with no spaces.\n");
    int count = 0;
    listStudentsByCourse(root, courseCode, &count);
    if (count == 0) printf("[INFO] No students registered in %s.\n", courseCode);
}

static void deleteRegistrationFlow(AVLNode** root) {
    if (!*root) {
        printf("[INFO] No student data available.\n");
        return;
    }
    char id[MAX_ID_LEN];
    getValidatedInput("Enter Student ID to modify: ", id, sizeof(id), isDigitsOnly, "[ERROR] Student ID must contain digits only.\n");
    AVLNode* student = findByID(*root, id);
    if (!student) {
        printf("[ERROR] Student not found.\n");
        return;
    }
    char courseCode[MAX_CODE_LEN];
    getValidatedInput("Enter Course Code to remove: ", courseCode, sizeof(courseCode), isCourseCode, "[ERROR] Course code must be alphanumeric with no spaces.\n");
    if (!removeCourseRecord(&student->data.courses, courseCode)) {
        printf("[ERROR] Course not found for this student.\n");
        return;
    }
    printf("[OK] Course removed.\n");
    if (!student->data.courses) {
        *root = deleteStudent(*root, id);
        printf("[OK] Student removed because no courses remain.\n");
    }
}

static void insertIntoHashFlow(HashTable* table, AVLNode* root) {
    if (!table->entries) {
        printf("[ERROR] Build the hash table first.\n");
        return;
    }
    char id[MAX_ID_LEN];
    getValidatedInput("Enter Student ID to hash: ", id, sizeof(id), isDigitsOnly, "[ERROR] Student ID must contain digits only.\n");
    AVLNode* student = findByID(root, id);
    if (!student) {
        printf("[ERROR] Student not found in AVL tree.\n");
        return;
    }
    if (hashInsert(table, student->data.name, student)) printf("[OK] Student inserted/updated in hash table.\n");
    else printf("[ERROR] Unable to insert into hash table.\n");
}

static void searchHashFlow(HashTable* table) {
    if (!table->entries) {
        printf("[ERROR] Hash table not initialized.\n");
        return;
    }
    char name[MAX_NAME_LEN];
    getValidatedInput("Enter Student Name to search: ", name, sizeof(name), isLettersSpaces, "[ERROR] Name must contain letters only.\n");
    int index = hashSearch(table, name);
    if (index < 0) {
        printf("[INFO] Name not found in hash table.\n");
    } else {
        HashEntry* entry = &table->entries[index];
        printf("Found at slot %d:\n", index);
        printStudent(entry->reference);
    }
}

static void deleteHashFlow(HashTable* table) {
    if (!table->entries) {
        printf("[ERROR] Hash table not initialized.\n");
        return;
    }
    char name[MAX_NAME_LEN];
    getValidatedInput("Enter Student Name to delete from hash table: ", name, sizeof(name), isLettersSpaces, "[ERROR] Name must contain letters only.\n");
    if (hashDelete(table, name)) printf("[OK] Entry removed from hash table.\n");
    else printf("[ERROR] Name not found in hash table.\n");
}

static void printHashInfo(void) {
    printf("Hash Function: h(s) = ((((0 * 131 + c1) * 131 + c2) ...) mod table_size), case-insensitive.\n");
}

int main() {
    AVLNode* root = NULL;
    HashTable table;
    table.entries = NULL;
    table.capacity = 0;
    table.size = 0;
    root = loadFromFile("reg.txt", root);
    int running = 1;
    while (running) {
        printf("\n============================================\n");
        printf("   COURSE REGISTRATION MANAGEMENT SYSTEM\n");
        printf("============================================\n");
        printf("1. Insert new registration\n");
        printf("2. Find student by name and update\n");
        printf("3. List students registered in a course\n");
        printf("4. Delete a student's registration\n");
        printf("5. Save AVL tree to reg.txt\n");
        printf("6. Build hash table and export students_hash.data\n");
        printf("7. Print hash table\n");
        printf("8. Print hash table size\n");
        printf("9. Show hash function description\n");
        printf("10. Insert student into hash table\n");
        printf("11. Search student in hash table\n");
        printf("12. Delete student from hash table\n");
        printf("13. Save hash table back to reg.txt\n");
        printf("14. Exit\n");
        int choice = readIntWithPrompt("Select option: ", 1, 14);
        switch (choice) {
            case 1:
                insertRegistrationFlow(&root);
                break;
            case 2:
                findStudentByNameFlow(root);
                break;
            case 3:
                listCourseFlow(root);
                break;
            case 4:
                deleteRegistrationFlow(&root);
                break;
            case 5:
                saveTreeToFile("reg.txt", root);
                break;
            case 6:
                buildHashTable(&table, root);
                break;
            case 7:
                printHashTable(&table);
                break;
            case 8:
                if (table.entries) printf("Hash table size: %d entries, %d capacity.\n", table.size, table.capacity);
                else printf("[INFO] Hash table not built yet.\n");
                break;
            case 9:
                printHashInfo();
                break;
            case 10:
                insertIntoHashFlow(&table, root);
                break;
            case 11:
                searchHashFlow(&table);
                break;
            case 12:
                deleteHashFlow(&table);
                break;
            case 13:
                saveHashTableToFile(&table, "reg.txt");
                break;
            case 14:
                running = 0;
                break;
        }
    }
    saveTreeToFile("reg.txt", root);
    freeTree(root);
    freeHashTable(&table);
    printf("[INFO] Goodbye.\n");
    return 0;
}
