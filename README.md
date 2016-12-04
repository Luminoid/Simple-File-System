# Simple File System

## File System Structure
### Super Block
Block1
``` {C}
int dir_index;  // Position of the directory infomation
int dir_len;    // File Number
int data_index; // Position of the first data block
```

### Dir