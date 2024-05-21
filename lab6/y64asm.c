#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

#define err_print(_s, _a...)            \
    do                                  \
    {                                   \
        if (lineno < 0)                 \
            fprintf(stderr, "[--]: "_s  \
                            "\n",       \
                    ##_a);              \
        else                            \
            fprintf(stderr, "[L%d]: "_s \
                            "\n",       \
                    lineno, ##_a);      \
    } while (0);

int64_t vmaddr = 0; /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8", REG_R8, 3},
    {"%r9", REG_R9, 3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}};
const reg_t *find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}

/* instruction set */
instr_t instr_set[] = {
    {"nop", 3, HPACK(I_NOP, F_NONE), 1},
    {"halt", 4, HPACK(I_HALT, F_NONE), 1},
    {"rrmovq", 6, HPACK(I_RRMOVQ, F_NONE), 2},
    {"cmovle", 6, HPACK(I_RRMOVQ, C_LE), 2},
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2},
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2},
    {"cmovne", 6, HPACK(I_RRMOVQ, C_NE), 2},
    {"cmovge", 6, HPACK(I_RRMOVQ, C_GE), 2},
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2},
    {"irmovq", 6, HPACK(I_IRMOVQ, F_NONE), 10},
    {"rmmovq", 6, HPACK(I_RMMOVQ, F_NONE), 10},
    {"mrmovq", 6, HPACK(I_MRMOVQ, F_NONE), 10},
    {"addq", 4, HPACK(I_ALU, A_ADD), 2},
    {"subq", 4, HPACK(I_ALU, A_SUB), 2},
    {"andq", 4, HPACK(I_ALU, A_AND), 2},
    {"xorq", 4, HPACK(I_ALU, A_XOR), 2},
    {"jmp", 3, HPACK(I_JMP, C_YES), 9},
    {"jle", 3, HPACK(I_JMP, C_LE), 9},
    {"jl", 2, HPACK(I_JMP, C_L), 9},
    {"je", 2, HPACK(I_JMP, C_E), 9},
    {"jne", 3, HPACK(I_JMP, C_NE), 9},
    {"jge", 3, HPACK(I_JMP, C_GE), 9},
    {"jg", 2, HPACK(I_JMP, C_G), 9},
    {"call", 4, HPACK(I_CALL, F_NONE), 9},
    {"ret", 3, HPACK(I_RET, F_NONE), 1},
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2},
    {"popq", 4, HPACK(I_POPQ, F_NONE), 2},
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1},
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2},
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4},
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8},
    {".pos", 4, HPACK(I_DIRECTIVE, D_POS), 0},
    {".align", 6, HPACK(I_DIRECTIVE, D_ALIGN), 0},
    {NULL, 1, 0, 0} // end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
        if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
            return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
    // 在符号表中搜索具有特定名称的符号，并返回找到的符号的指针。如果没有找到符号，则返回NULL。
    symbol_t *current_symbol = symtab->next;
    // 获取待查找名称的长度
    int name_len = strlen(name);
    while (current_symbol != NULL)
    {
        // 获取当前符号名称的长度
        int current_name_len = strlen(current_symbol->name);

        // 首先检查两个名称的长度是否相等
        if (current_name_len == name_len)
        {
            // 如果长度相同，则比较字符串内容
            if (strncmp(current_symbol->name, name, name_len) == 0)
            {
                return current_symbol; // 找到匹配，返回当前符号
            }
        }
        current_symbol = current_symbol->next; // 移至下一个符号进行检查
    }
    return NULL; // 如果没有找到匹配的符号，返回NULL
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    // 创建新符号并添加到符号表
    /* check duplicate */
    if (find_symbol(name) != NULL)
    {
        return -1;
    }
    /* create new symbol_t (don't forget to free it)*/
    symbol_t *tmp = (symbol_t *)malloc(sizeof(symbol_t));
    tmp->name = strdup(name);
    tmp->addr = vmaddr;
    tmp->next = NULL;
    /* add the new symbol_t to symbol table */
    tmp->next = symtab->next;
    symtab->next = tmp;
    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 */
void add_reloc(char *name, bin_t *bin)
{
    // 在将新的重定位条目添加到重定位表中
    /* create new reloc_t (don't forget to free it)*/
    reloc_t *tmp = (reloc_t *)malloc(sizeof(reloc_t));
    /* add the new reloc_t to relocation table */
    tmp->name = strdup(name);
    tmp->y64bin = bin;
    tmp->next = reltab->next;
    reltab->next = tmp;
}

/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s) >= '0' && *(s) <= '9') || *(s) == '-' || *(s) == '+')
#define IS_LETTER(s) ((*(s) >= 'a' && *(s) <= 'z') || (*(s) >= 'A' && *(s) <= 'Z'))
#define IS_COMMENT(s) (*(s) == '#')
#define IS_REG(s) (*(s) == '%')
#define IS_IMM(s) (*(s) == '$')

#define IS_BLANK(s) (*(s) == ' ' || *(s) == '\t')
#define IS_END(s) (*(s) == '\0')

#define SKIP_BLANK(s)                     \
    do                                    \
    {                                     \
        while (!IS_END(s) && IS_BLANK(s)) \
            (s)++;                        \
    } while (0);

/* return value from different parse_xxx function */
typedef enum
{
    PARSE_ERR = -1,
    PARSE_REG,
    PARSE_DIGIT,
    PARSE_SYMBOL,
    PARSE_MEM,
    PARSE_DELIM,
    PARSE_INSTR,
    PARSE_LABEL
} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    // 解析Y64汇编代码中的一个指令
    // 更新ptr和inst
    /* skip the blank */
    char *tmpPtr = *ptr;
    instr_t *tmpInst = *inst;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    /* find_instr and check end */
    tmpInst = find_instr(tmpPtr);
    if (tmpInst == NULL)
    {
        return PARSE_ERR;
    }
    tmpPtr += tmpInst->len;
    // 最后应该是空格或者结束
    if (IS_END(tmpPtr) || IS_BLANK(tmpPtr))
    {
        /* set 'ptr' and 'inst' */
        *ptr = tmpPtr;
        *inst = tmpInst;
        return PARSE_INSTR;
    }
    return PARSE_ERR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    // 检查字符串中的当前位置是否存在指定的分隔符，如果存在，则成功解析并更新字符串的解析位置。
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    // 检查当前位置是不是指定的分隔符
    if (*tmpPtr != delim)
    {
        err_print("Invalid \'%c\'", delim);
        return PARSE_ERR;
    }
    /* set 'ptr' */
    tmpPtr += 1;
    *ptr = tmpPtr;
    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token,
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    // 解析Y64汇编语言中的寄存器标记（例如%eax），并在成功解析时更新字符串指针ptr以及寄存器标识符regid。
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    /* find register */
    const reg_t *tmpRegid = find_register(tmpPtr);
    if (tmpRegid == NULL)
    {
        err_print("Invalid REG");
        return PARSE_ERR;
    }
    /* set 'ptr' and 'regid' */
    tmpPtr += tmpRegid->namelen;
    *ptr = tmpPtr;
    *regid = tmpRegid->id;
    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    // 解析Y64汇编代码中期望的符号标记（例如，一个函数名Main）
    // 并在成功解析后更新字符串指针ptr以及动态分配内存并存储符号名称到name。
    char *cur = *ptr;
    /* Skip leading whitespace */
    SKIP_BLANK(cur);
    if (IS_END(cur) || (IS_DIGIT(cur)))
    {
        return PARSE_ERR;
    }
    char *start = cur;
    // 计算symbol的长度
    while (!IS_END(cur) && (IS_LETTER(cur) || IS_DIGIT(cur)))
    {
        cur++;
    }
    int length = cur - start;
    if (length == 0)
    {
        return PARSE_ERR;
    }
    *name = (char *)malloc((length + 1) * sizeof(char));
    if (*name == NULL)
    {
        return PARSE_ERR;
    }
    strncpy(*name, start, length);
    (*name)[length] = '\0';
    *ptr = cur;
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    if (IS_DIGIT(tmpPtr) == FALSE)
    {
        return PARSE_ERR;
    }
    /* calculate the digit, (NOTE: see strtoll()) */
    char *endptr = tmpPtr;
    // printf("%s\n",tmpPtr);
    // unsigned long val1 = strtoll(tmpPtr, &endptr, 0);
    unsigned long long val1 = strtoull(tmpPtr, &endptr, 0);
    long val=(long)val1;
    /* set 'ptr' and 'value' */
    // printf("%lx \n",val1);
    *value = val;
    *ptr = endptr;
    return PARSE_DIGIT;
}


/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr))
    {
        return PARSE_ERR;
    }
    parse_t result = PARSE_ERR;
    /* if IS_IMM, then parse the digit */
    if (IS_IMM(tmpPtr))
    {
        tmpPtr++; // 跳过 '$'
        if (!IS_DIGIT(tmpPtr))
        {
            err_print("Invalid Immediate");
            return PARSE_ERR;
        }
        result = parse_digit(&tmpPtr, value);
    }
    /* if IS_LETTER, then parse the symbol */
    else if (IS_LETTER(tmpPtr))
    {
        result = parse_symbol(&tmpPtr, name);
    }
    if (result == PARSE_ERR)
    {
        err_print("Invalid Immediate");
        return PARSE_ERR;
    }
    /* set 'ptr' and 'name' or 'value' */
    *ptr = tmpPtr;
    return result;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    /* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
    long tmpValue = 0;
    regid_t *tmpRegid = regid;
    // 先提取前面的数
    // 可能数不存在
    if (IS_DIGIT(tmpPtr))
    {
        if (parse_digit(&tmpPtr, &tmpValue) == PARSE_ERR)
        {
            return PARSE_ERR;
        }
    }

    // 再检验(
    if (*tmpPtr != '(')
    {
        err_print("Invalid MEM");
        return PARSE_ERR;
    }
    else
    {
        tmpPtr++;
        if (parse_reg(&tmpPtr, tmpRegid) == PARSE_ERR)
        {
            return PARSE_ERR;
        }
        if (*tmpPtr != ')')
        {
            err_print("Invalid MEM");
            return PARSE_ERR;
        }
        tmpPtr++;
    }
    /* set 'ptr', 'value' and 'regid' */
    *ptr = tmpPtr;
    *value = tmpValue;
    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    parse_t result = PARSE_ERR;
    char *tmpName;
    long tmpValue;
    /* if IS_DIGIT, then parse the digit */
    if (IS_DIGIT(tmpPtr) == TRUE)
    {
        result = parse_digit(&tmpPtr, &tmpValue);
        if (result == PARSE_ERR)
        {
            return PARSE_ERR;
        }
    }
    /* if IS_LETTER, then parse the symbol */
    if (IS_LETTER(tmpPtr) == TRUE)
    {
        result = parse_symbol(&tmpPtr, &tmpName);
        if (result == PARSE_ERR)
        {
            return PARSE_ERR;
        }
    }
    /* set 'ptr', 'name' and 'value' */
    *ptr = tmpPtr;
    *name = tmpName;
    *value = tmpValue;
    return result;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name) // unsolved
{
    /* skip the blank and check */
    char *tmpPtr = *ptr;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        return PARSE_ERR;
    }
    /* allocate name and copy to it */
    parse_t result = parse_symbol(&tmpPtr, name);
    if (result == PARSE_ERR)
    {
        return PARSE_ERR;
    }
    if (*(tmpPtr) != ':')
    {
        return PARSE_ERR;
    }
    /* set 'ptr' and 'name' */
    tmpPtr++;
    *ptr = tmpPtr;
    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     TYPE_XXX: success, fill line_t with assembled y64 code
 *     TYPE_ERR: error, try to print err information
 */
type_t parse_line(line_t *line) // unsolved
{

    /* when finish parse an instruction or lable, we still need to continue check
     * e.g.,
     *  Loop: mrmovl (%rbp), %rcx
     *           call SUM  #invoke SUM function */

    /* skip blank and check IS_END */
    char *tmpPtr = line->y64asm;
    SKIP_BLANK(tmpPtr);
    if (IS_END(tmpPtr) == TRUE)
    {
        line->type = PARSE_ERR;
        return PARSE_ERR;
    }
    /* is a comment ? */
    // 如果是注释，那么直接退出，返回TYPE_COMM
    if (IS_COMMENT(tmpPtr))
    {
        line->type = TYPE_COMM;
        return TYPE_COMM;
    }
    /* is a label ? */
    char *tmp = tmpPtr;
    char *name = NULL;
    instr_t *tmpInstr;
    // 判断是否为instruction
    if (parse_instr(&tmpPtr, &tmpInstr) != PARSE_INSTR)
    {
        // 不是一个instr
        // 复位tmpPtr
        tmpPtr = tmp;
        // 判断是不是一个label
        if (parse_label(&tmpPtr, &name) == PARSE_LABEL)
        {
            // 是一个label
            // 判断是不是会已经存在
            if (add_symbol(name) != 0)
            {
                err_print("Dup symbol:%s", name);
                line->type = TYPE_ERR;
                return line->type;
            }
        }
        else
        {
            // 既不是instr也不是label
            line->type = TYPE_ERR;
            return line->type;
        }

        // 可能会有第二个指令
        SKIP_BLANK(tmpPtr);
        if (IS_END(tmpPtr) || IS_COMMENT(tmpPtr))
        {
            line->y64bin.addr = vmaddr;
            line->y64bin.bytes = 0;
            line->type = TYPE_INS;
            return line->type;
        }
        else
        {
            parse_t result = parse_instr(&tmpPtr, &tmpInstr);
            if (result == PARSE_ERR)
            {
                line->type = result;
                return result;
            }
        }
    }
    /* is an instruction ? */
    itype_t type = HIGH(tmpInstr->code);
    bin_t bin;
    // 首先，处理一个byte的指令，即halt nop ret
    if (type == I_HALT || type == I_NOP || type == I_RET)
    {
        // 没有后面的字节，无需操作
    }
    // 然后，处理两个byte的指令，即rrmovq opq pushq popq
    if (type == I_RRMOVQ || type == I_ALU)
    {
        regid_t reg_a;
        regid_t reg_b;
        // 首先，检验是否有第一个寄存器
        if (parse_reg(&tmpPtr, &reg_a) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验是不是有,
        if (parse_delim(&tmpPtr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验第二个寄存器
        if (parse_reg(&tmpPtr, &reg_b) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }

        // 合成寄存器代码
        bin.codes[1] = HPACK(reg_a, reg_b);
    }
    if (type == I_POPQ || type == I_PUSHQ)
    {
        regid_t reg_a;
        regid_t reg_b = REG_NONE;
        // 首先，检验是否有第一个寄存器
        if (parse_reg(&tmpPtr, &reg_a) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 合成寄存器代码
        bin.codes[1] = HPACK(reg_a, reg_b);
    }

    // jmp call 指令
    if (type == I_JMP || type == I_CALL)
    {
        if (parse_symbol(&tmpPtr, &name) == PARSE_SYMBOL)
        {
            add_reloc(name, &(line->y64bin));
        }
        else
        {
            err_print("Invalid DEST");
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
    }

    // irmovq rmmovq mrmovq 指令
    // char *name;
    long value;
    if (type == I_IRMOVQ)
    {
        regid_t reg_a = REG_NONE;
        regid_t reg_b;
        // 首先，检验是否有imm
        switch (parse_imm(&tmpPtr, &name, &value))
        {
        case PARSE_ERR:
            line->type = TYPE_ERR;
            return TYPE_ERR;
        case PARSE_SYMBOL:
            add_reloc(name, &line->y64bin);
            break;
        case PARSE_DIGIT:
            for (int i = 0; i < 8; i++)
            {
                bin.codes[i + 2] = value & 0xFF;
                value = value >> 8;
            }
            break;
        default:
            break;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验是不是有,
        if (parse_delim(&tmpPtr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验第二个寄存器
        if (parse_reg(&tmpPtr, &reg_b) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        bin.codes[1] = HPACK(reg_a, reg_b);
    }

    if (type == I_RMMOVQ)
    {
        regid_t reg_a;
        regid_t reg_b;
        // 首先，检验是否有第一个寄存器
        if (parse_reg(&tmpPtr, &reg_a) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验是不是有,
        if (parse_delim(&tmpPtr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验第二个寄存器
        if (parse_mem(&tmpPtr, &value, &reg_b) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        bin.codes[1] = HPACK(reg_a, reg_b);
        for (int i = 0; i < 8; i++)
        {
            bin.codes[i + 2] = value & 0xFF;
            value = value >> 8;
        }
    }

    if (type == I_MRMOVQ)
    {
        regid_t reg_a;
        regid_t reg_b;
        // 首先，检验是否有第一个寄存器
        if (parse_mem(&tmpPtr, &value, &reg_b) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验是不是有,
        if (parse_delim(&tmpPtr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        SKIP_BLANK(tmpPtr);
        // 再检验第二个寄存器
        if (parse_reg(&tmpPtr, &reg_a) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        bin.codes[1] = HPACK(reg_a, reg_b);
        for (int i = 0; i < 8; i++)
        {
            bin.codes[i + 2] = value & 0xFF;
            value = value >> 8;
        }
    }
    if (type != I_DIRECTIVE)
    {
        bin.addr = vmaddr;
        bin.bytes = tmpInstr->bytes;
        bin.codes[0] = tmpInstr->code;
        line->type = TYPE_INS;
        line->y64bin = bin;
        vmaddr += tmpInstr->bytes;
    }
    if (type == I_DIRECTIVE)
    {
        dtv_t dtv = LOW(tmpInstr->code);
        if (dtv == D_ALIGN)
        {
            // 尝试解析后面的数字
            SKIP_BLANK(tmpPtr);
            if (parse_digit(&tmpPtr, &value) == PARSE_ERR)
            {
                line->type = TYPE_ERR;
                return TYPE_ERR;
            }
            vmaddr = (vmaddr + value - 1) / value * value;
            line->y64bin.addr = vmaddr;
            line->type = TYPE_INS;
            line->y64bin.bytes = 0;
        }
        if (dtv == D_POS)
        {
            // 尝试解析后面的数字
            SKIP_BLANK(tmpPtr);
            if (parse_digit(&tmpPtr, &value) == PARSE_ERR)
            {
                line->type = TYPE_ERR;
                return TYPE_ERR;
            }
            vmaddr = value;
            line->y64bin.addr = vmaddr;
            line->type = TYPE_INS;
            line->y64bin.bytes = 0;
        }
        if (dtv == D_DATA)
        {
            switch (parse_data(&tmpPtr, &name, &value))
            {
            case PARSE_ERR:
                line->type = TYPE_ERR;
                return TYPE_ERR;
            case PARSE_DIGIT:
                // printf("%lx \n", value);
                for (int i = 0; i < tmpInstr->bytes; i++)
                {
                    line->y64bin.codes[i] = value & 0xFF;
                    value = value >> 8;
                    // printf("%02x ", line->y64bin.codes[i]);
                }
                // printf("\n");
                break;
            case PARSE_SYMBOL:
                add_reloc(name, &line->y64bin);
                break;
            default:
                break;
            }
            line->y64bin.addr = vmaddr;
            line->y64bin.bytes = tmpInstr->bytes;
            line->type = TYPE_INS;
            vmaddr += tmpInstr->bytes;
        }
    }
    return line->type;
}
/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL)
    {
        slen = strlen(asm_buf);
        while ((asm_buf[slen - 1] == '\n') || (asm_buf[slen - 1] == '\r'))
        {
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno++;

        if (parse_line(line) == TYPE_ERR)
        {
            return -1;
        }
    }

    lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
    rtmp = reltab->next;
    byte_t *tmpByte;
    while (rtmp)
    {
        /* find symbol */
        if (find_symbol(rtmp->name) == NULL)
        {
            err_print("Unknown symbol:'%s'", rtmp->name);
            return -1;
        }
        int num = 0;
        /* relocate y64bin according itype */
        switch (HIGH(rtmp->y64bin->codes[0]))
        {
        case I_JMP:
            num = 1;
            break;
        case I_CALL:
            num = 1;
            break;
        case I_IRMOVQ:
            num = 2;
            break;
        default:
            break;
        }
        tmpByte = &rtmp->y64bin->codes[num];
        // 转为little endian
        for (int i = 0; i < 8; ++i)
        {
            tmpByte[i] = ((find_symbol(rtmp->name)->addr >> (i * 8)) & 0xff);
        }
        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
    line_t *tmp = line_head->next;
    while (tmp)
    {
        if (tmp->type == TYPE_INS)
        {
            /* binary write y64 code to output file (NOTE: see fwrite()) */
            bin_t bin = tmp->y64bin;
            fseek(out, bin.addr, SEEK_SET);                    // 定位到指令应该写入的位置
            fwrite(bin.codes, sizeof(byte_t), bin.bytes, out); // 写入指令的二进制代码
        }
        tmp = tmp->next;
    }
    return 0;
}

/* whether print the readable output to screen or not ? */
bool_t screen = FALSE;

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        char c;
        int h = (value >> 4 * i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len - i - 1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS)
    {
        bin_t *y64bin = &line->y64bin;
        int i;

        strcpy(buf, "  0x000:                      | ");

        hexstuff(buf + 4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf + 9 + 2 * i, y64bin->codes[i] & 0xFF, 2);
    }
    else
    {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/*
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL)
    {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do
    {
        rtmp = reltab->next;
        if (reltab->name)
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);

    symbol_t *stmp = NULL;
    do
    {
        stmp = symtab->next;
        if (symtab->name)
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do
    {
        ltmp = line_head->next;
        if (line_head->y64asm)
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;

    if (argc < 2)
        usage(argv[0]);

    if (argv[nextarg][0] == '-')
    {
        char flag = argv[nextarg][1];
        switch (flag)
        {
        case 'v':
            screen = TRUE;
            nextarg++;
            break;
        default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg]) - 3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg] + rootlen, ".ys"))
        usage(argv[0]);

    if (rootlen > 500)
    {
        err_print("File name too long");
        exit(1);
    }

    /* init */
    init();

    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname + rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in)
    {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }

    if (assemble(in) < 0)
    {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);

    /* relocate binary code */
    if (relocate() < 0)
    {
        err_print("Relocate binary code error");
        exit(1);
    }

    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname + rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out)
    {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0)
    {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);

    /* print to screen (.yo file) */
    if (screen)
        print_screen();

    /* finit */
    finit();
    return 0;
}