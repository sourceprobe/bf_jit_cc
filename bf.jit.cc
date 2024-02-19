#include <unordered_map>
#include <stdio.h>
#include <string>
#include <vector>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <malloc.h>
#include "mm.h"
#include <cassert>

using std::unordered_map;
using std::string;
using std::vector;

typedef unsigned char byte;
typedef vector<byte> Bytes;

const int TAPE_LEN = 30*1000;
struct Jumps {
    // forward maps from a forward brace index
    // to the matching closing brace.
    unordered_map<int, int> forward;
    // backward maps from closing brace index
    // to the matching opening brace
    unordered_map<int, int> backward;
};

int fatal(const char* msg) {
    puts(msg);
    exit(1);
    return 0;
}
    
Jumps build_jumps(const string& prog) {
    /*
    build tables to lookup the matching
    brace for a program.
    */
    Jumps result;
    vector<int> stack;
    int i = 0;
    while(i < prog.size()) {
        if (prog[i] == '[') {
            stack.push_back(i);
        } else if (prog[i] == ']') {
            if (stack.size() == 0) {
                puts("no [ on stack.");
                exit(1);
            }
            int match = stack.back(); stack.pop_back();
            result.backward[i] = match;
            result.forward[match] = i;
        }
        i += 1;
    }
    if (stack.size() > 0) {
        puts("jump stack not empty");
        exit(1);
    }
    return result;
}

int jump(const unordered_map<int, int>& jumps,
    int index) {
    auto it = jumps.find(index);
    if (it != jumps.end()) {
        return it->second;
    } else {
        printf("no jumps for %d\n", index);
        exit(1);
    }
}

extern "C" {
    char tape[TAPE_LEN];
    int i = 0;
    int pc = 0;
    void plus(void);
    void plus_end(void);
    void minus(void);
    void minus_end(void);
    void right(void);
    void right_end(void);
    void left(void);
    void left_end(void);
    void dot(void);
    void dot_end(void);
    void prologue(void);
    void prologue_end(void);
    // branches. left and right brace characters.
    void bleft(void);
    void bleft_end(void);
    void bright(void);
    void bright_end(void);
}

// Assembled native code for each instruction.
typedef unordered_map<char, Bytes> Instructions;
Instructions instructions;
Bytes& instruction(char instc) {
    auto it = instructions.find(instc);
    if (it != instructions.end()) {
        return it->second;
    } else {
        printf("unknown instruction: %c\n", instc);
        exit(1);
    }
}

Bytes copy_range(void (*start)(void),void (*end)(void)) {
    int len = (byte*)end - (byte*)start;
    Bytes result;
    result.resize(len);
    memcpy(result.data(), (void*)start, len);
    for(int i = 0; i < result.size(); i++) {
        printf("%x ", result[i]);
    }
    puts("");
    return result;
}
void init_instructions() {
    instructions['+'] = copy_range(plus, plus_end);
    instructions['-'] = copy_range(minus, minus_end);
    instructions['<'] = copy_range(left, left_end);
    instructions['>'] = copy_range(right, right_end);
    instructions['.'] = copy_range(dot, dot_end);
    instructions['['] = copy_range(bleft, bleft_end);
    instructions[']'] = copy_range(bright, bright_end);
    // end, prologue + ret used at end of block
    instructions['e'] = copy_range(prologue, prologue_end);
}
void jit_init() {
    init_instructions();
    printf("+ len: %x\n", instruction('+').size());
    printf("- len: %x\n", instruction('-').size());
    printf("< len: %x\n", instruction('<').size());
    printf("> len: %x\n", instruction('>').size());
    printf(". len: %x\n", instruction('.').size());
    printf("e len: %x\n", instruction('e').size());
    printf("pagesize: %d\n", getpagesize());

    puts("jit setup complete");
}

struct Block {
    void *code;
    // instruction count
    int count;
};

// mapping from pc to compiled blocks. 
unordered_map<int, Block*> block_cache;

static int alloc_bytes = 0;
static MM memory;

void run_block(Block* data) {
    void (*block)(void);
    block = reinterpret_cast<void(*)(void)>(data->code);
    block();
    pc += data->count;
}

// Rewrite the last 4 bytes at code_output  to the jump offset.
void fixup_last_branch(
    int pc_dest,
    byte* code_output,
    int code_length,
    const unordered_map<int, int>& jump_offsets) {
    // x86_64 always has eip set to the following instruction. 
    // to and from are relative to the start of the block.
    auto dst = jump_offsets.find(pc_dest);
    int to = dst != jump_offsets.end() 
        ? dst->second 
        : fatal("no jump");
    int from = code_length;
    int32_t delta = to - from;
    memcpy(code_output - 4, &delta, sizeof(delta));
}

    
Block* compile_block(const string& prog, int pc, const Jumps& jumps) {
    int start = pc;
    int bytes = 0;
    Block* block = block_cache[pc] = new Block;
    assert(pc == 0);

    // pc -> byte offset in block of the following instruction
    unordered_map <int, int> jump_offsets; 
  
    while (pc < prog.length()) {
        char inst = prog[pc];
        bytes += instruction(inst).size();
        jump_offsets[pc] = bytes;
        pc++;
    }
    bytes += instruction('e').size();
    block->count = pc - start;

    byte *code;
    memory.alloc((void**)&code, bytes);
    alloc_bytes += bytes;
    byte *write = code;
    // body
    int offset = 0;
    while (start < pc) {
        char instc = prog[start];
        Bytes &instn = instruction(instc);
        memcpy(write + offset, instn.data(), instn.size());

        offset += instn.size();
        if (instc == '[') {
            fixup_last_branch(jump(jumps.forward, start),
                write+offset, offset, jump_offsets);
        } else if (instc == ']') {
            fixup_last_branch(jump(jumps.backward, start),
                write+offset, offset, jump_offsets);
        }
        start++;
    }
    // prologue
    {
        Bytes &instn = instruction('e');
        memcpy(write + offset, instn.data(), instn.size());
    }
    // printf("block at %x, len=%d\n", code, bytes);

    block->code = reinterpret_cast<void*>(code);
    
    return block;
}
    
void bf(const string& prog) {
    memset(tape, TAPE_LEN, 0);
    Jumps jumps = build_jumps(prog);
    int len = prog.length();
    // One block now represents the entire program.
    // The ret at the end represents the end of program.
    Block* block = compile_block(prog, pc, jumps);
    run_block(block);
}

string slurp(const string& path) {
    string output;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        perror("failed to open file");
        exit(1);
    }
    char buffer[1024];
    int goal = sizeof(buffer)-1;
    while(true) {
        size_t n = fread(buffer, 1, goal, f);
        if (n < 0) {
            perror("file read error");
            exit(1);
        } 
        buffer[n] = 0;
        output.append(buffer, n);
        if (n < goal)
            break;
    }
    return output;
}

string clean(const string& input) {
    string program;
    program.reserve(input.length());
    int i = 0;
    while (i < input.length()) {
        char c = input[i];
        if (c == ';') {
            // drop until newline
            while (i < input.length() && 
                input[i] != '\n') {
                i++;
            }
            continue;
        }

        switch(c) {
            case '\n':
                [[fallthrough]];
            case ' ':
                break;
            default:
                program.push_back(c);
        }
        i++;
    }
    return program;
}
    
int main(int argc, char** argv) {
    if (argc < 2) {
        puts("usage: ./bf file.bf");
        return 0;
    }
    jit_init();
    string program = slurp(argv[1]);
    program = clean(program);
    bf(program);
    puts("done");
    printf("jit stats: \n");
    printf("alloc_bytes: %d\n", alloc_bytes);
    printf("num blocks: %d\n", block_cache.size());
    return 0;
}
