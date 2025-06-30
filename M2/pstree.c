#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "stdlib.h"
#include "unistd.h"
#include "getopt.h"
#include "dirent.h"
#include "ctype.h"

#define HASH_SIZE 10007
#define MAX_DEPTH 1000

// process node
typedef struct process_node{
    pid_t pid;
    pid_t ppid;
    char name[256];

    struct process_node **children;
    int child_count;
    int child_capacity;

}process_node_t;

// hash table elem
typedef struct hash_entry {
    pid_t pid;
    process_node_t *process;
    struct hash_entry *next; // 链表法解决冲突
} hash_entry_t;

// 定义长选项
// longopts 数组以 {0, 0, 0, 0} 结构体作为终止符
static struct option long_options[] = {
    {"show-pids", no_argument, 0, 'p'},
    {"numeric-sort", no_argument, 0, 'n'},
    {"version", no_argument, 0, 'V'},
    {0, 0, 0, 0}
};

hash_entry_t *hash_table[HASH_SIZE];
process_node_t *root_process = NULL;


// hash function
unsigned int hash_function(pid_t pid) {
    return (unsigned int)pid % HASH_SIZE;
}

// init node
process_node_t* creat_process_node(pid_t pid, pid_t ppid, const char *name) {
    process_node_t *node = malloc(sizeof(process_node_t));
    if(!node) return NULL;

    node->pid = pid;
    node->ppid = ppid;
    strncpy(node->name, name, sizeof(node->name) -1);
    node->name[sizeof(node->name) - 1] = '\0';

    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;

    return node;
}

void hash_insert(pid_t pid, pid_t ppid, const char* name) {
    unsigned int index = hash_function(pid);

    hash_entry_t *entry = malloc(sizeof(hash_entry_t));
    if(!entry) return;

    entry->pid = pid;
    entry->process = creat_process_node(pid, ppid, name);
    //头插法，实际和链表插入一样的 hash_table 为一组链表的指针
    // 新来的节点直接插向这组节点的头部
    // hash_table 指向新节点 
    entry->next = hash_table[index];
    hash_table[index] = entry;

    if (pid == 1) {
        root_process = entry->process;
    }

}

// 清理哈希表内存
void cleanup_hash_table() {
    for (int i = 0; i < HASH_SIZE; i++) {
        hash_entry_t *entry = hash_table[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            
            // 释放子进程数组
            if (entry->process->children) {
                free(entry->process->children);
            }
            
            // 释放进程节点
            free(entry->process);
            
            // 释放哈希表项
            free(entry);
            
            entry = next;
        }
        hash_table[i] = NULL;
    }
}

// 查找节点
process_node_t *hash_find(pid_t pid) {
    unsigned int index = hash_function(pid);
    hash_entry_t *entry = hash_table[index];

    while(entry) {
        if (entry->pid == pid) {
            return entry->process;
        }
        entry = entry->next;
    }
    return NULL;
}

void add_child(process_node_t *parent, process_node_t *child) {
    if (!parent || !child) return;

    // 动态扩展children数组  vector
    if (parent->child_count >= parent->child_capacity) {
        int new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        parent->children = realloc(parent->children, new_capacity * sizeof(process_node_t));

        if (!parent->children) return;

        parent->child_capacity = new_capacity;
    }

    // children is an array about node 
    parent->children[parent->child_count++] = child;
}

void build_process_tree(){
    for (int i = 0; i < HASH_SIZE; ++i) {
        hash_entry_t *entry = hash_table[i];
        while(entry) {
            process_node_t *child = entry->process;
            process_node_t *parent = hash_find(child->ppid);

            if(parent) {
                add_child(parent, child);
            } else if(child->ppid != 0){
                //托孤
                if (root_process && child->pid != 1) {
                    add_child(root_process, child);
                }
            }
            entry = entry->next;
        }
    }
}


int parse_proc_stat(pid_t pid, process_node_t *proc_info) {

    char filename[64];
    char state;
    

    // construct filename
    snprintf(filename, sizeof(filename), "/proc/%d/stat", pid);

    FILE *fp = fopen(filename, "r");
    // maybe process has end
    if (!fp) {
        return -1;
    }

     // 解析关键字段 保留前四个有效数据
    if (fscanf(fp, "%d (%255[^)]) %c %d", &proc_info->pid, proc_info->name, &state, &proc_info->ppid) != 4) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}



// C return a int to express true or false
int is_number(const char *str) {
    if (*str == '\0') return 0;  // null string

    while(*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }

    return 1;
}

void collect_all_process() {

    DIR *proc_dir;
    struct dirent *entry;
    process_node_t proc_info;
    
    memset(hash_table, 0, sizeof(hash_table));

    // open dir
    proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("opendir /proc");
        return;
    }

    // traverse_proc
    while((entry = readdir(proc_dir)) != NULL) {
        if (is_number(entry->d_name)) {
            
            pid_t pid = atoi(entry->d_name);
            if (parse_proc_stat(pid, &proc_info) == 0) {
                // save data
                hash_insert(proc_info.pid, proc_info.ppid, proc_info.name);
            }
        }
    }
    closedir(proc_dir);
    build_process_tree();
}


// 比较函数，用于qsort
int compare_pids(const void *a, const void *b) {
    process_node_t *proc_a = *(process_node_t **)a;
    process_node_t *proc_b = *(process_node_t **)b;
    return proc_a->pid - proc_b->pid;
}

// 对子进程按PID排序
void sort_children_by_pid(process_node_t *node) {
    if (node->child_count <= 1) return;
    
    qsort(node->children, node->child_count, sizeof(process_node_t *), compare_pids);
}


void print_tree_prefix(int is_last[], int depth){

    for (int i = 0; i < depth - 1; ++i) {
        if (is_last[i]) {
            printf("    ");
        } else {
            printf("|   ");
        }
    }
    if(depth > 0) {
        if (is_last[depth - 1]) {
            printf("+-- ");
        }else {
            printf("+-- ");
        }
    }

}


void print_tree(process_node_t *node, int is_last[], int depth, 
                     int show_pids, int numeric_sort) {
    
    if (!node) return;

    print_tree_prefix(is_last, depth);
    
    
    if(show_pids) {
        printf("%s(%d)\n", node->name, node->pid);
    } else{
        printf("%s\n", node->name);
    }

    if (numeric_sort && node->child_count > 0) {
        sort_children_by_pid(node);
    }


    for (int i = 0; i < node->child_count; i++) {
        // 标记当前子进程是否是最后一个
        is_last[depth] = (i == node->child_count - 1);
        
        print_tree(node->children[i], is_last, depth + 1, 
                        show_pids, numeric_sort);
    }
}

void pstree_print_with_lines(int show_pids, int numeric_sort) {
    if (!root_process) {
        printf("Error: No root process found!\n");
        return;
    }
    
    int is_last[MAX_DEPTH] = {0};  // 初始化为false
    print_tree(root_process, is_last, 0, show_pids, numeric_sort);
}




int main(int argc, char *argv[]) {
    
    int opt = 0;
    int show_pids = 0;
    int numeric_sort = 0;
    int version_info = 0;

    int long_index = 0;

    while((opt = getopt_long(argc, argv, "pnV", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'p':
                show_pids = 1;
                
                break;
            case 'n':
                numeric_sort = 1;
                
                break;
            case 'V':
                version_info = 1;
                
                break;
            case '?':
                fprintf(stderr, "Usage: %s [-p|--show-pids] [-n|--numeric-sort] [-V|--version]\n", argv[0]);
                return EXIT_FAILURE;
            default:
                abort();
        }
    }

// 处理版本信息
    if (version_info) {
        printf("pstree version 1.0\n");
        return EXIT_SUCCESS;
    }
    
    // 核心功能：收集进程并打印树
    collect_all_process();
    
    if (!root_process) {
        fprintf(stderr, "Error: Could not find root process (PID 1)\n");
        return EXIT_FAILURE;
    }
    // 打印进程树
    pstree_print_with_lines(show_pids, numeric_sort);
    
    // 清理资源（可选，程序结束时系统会自动回收）
    cleanup_hash_table();
  return 0;
}
