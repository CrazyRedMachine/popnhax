#include <cstdio>
#include <cstring>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "util/bst.h"
#include "util/log.h"
#include "util/patch.h"
#include "util/search.h"
#include "libdisasm/libdis.h"

#include "omnimix_patch.h"

#define LINE_SIZE 512

#define MUSIC_IDX  0
#define CHART_IDX  1
#define STYLE_IDX  2
#define FLAVOR_IDX 3
#define CHARA_IDX  4
#define NUM_IDX    5

bst_t *g_xref_bst = NULL;
bst_t *g_data_xref_bst = NULL;
bst_t *g_bad_offset_bst = NULL;

#if DEBUG==1
	#define membuf_printf(membuf, ...) LOG(__VA_ARGS__)
#endif

// These offsets are known bads, you can add new offsets either in there or in data_mods\patches_blacklist.dat
uint32_t g_offset_blacklist[] = {
    0x100be154,
    0x100be346,
    0x100bed91,
    0x100e56ec,
    0x100e56f5,
    0x100e5a55,
    0x100e5a5e,
    0x100fa4e2
};

/* extra_data for bst */
typedef struct offset_list_s {
    uint32_t offset;
    struct offset_list_s *next;
    struct offset_list_s *last;
} offset_list_t;

static void print_list(offset_list_t *list)
{
    int depth = 0;
    offset_list_t *curr_elem = list;
    while ( curr_elem )
    {
        LOG("-> offset %0X ", curr_elem->offset );
        depth++;
        curr_elem = curr_elem->next;
    }
    LOG("\ndepth %d\n\n", depth);
}

/* extra_data_cb */
void* add_to_offset_list(void *list, void *new_tail)
{
    offset_list_t *curr_elem = ((offset_list_t *)list)->last;
    curr_elem->next = (offset_list_t *)new_tail;
    ((offset_list_t *)list)->last = (offset_list_t *)new_tail;

    return list;
}

typedef struct table_s {
    char *name;
    uint32_t *addr;
    uint32_t size; // size of one entry
    uint32_t limit; // number of entries
} table_t;

table_t g_tables[NUM_IDX];

typedef struct update_patches_s {
    uint8_t idx;
    uint8_t patch_method;
    uint32_t value;
    uint32_t offset; // optional, for weird patches
} update_patches_t;

static uint32_t *_find_binary_ex(char *data, DWORD dllSize, const char *search_pattern, uint32_t search_size, uint32_t search_head, int8_t search_idx, bool wildcards)
{
    //fprintf(stderr, "find binary %.*s\n", search_size, search_pattern);
    uint32_t *ea = 0;
    int8_t found = -1;
    while (true)
    {
        int64_t pattern_offset;
        if (wildcards)
            pattern_offset = wildcard_search(data, dllSize -((uint32_t) ea)-1, search_pattern, search_size, ((uint32_t) ea)+1);
        else
            pattern_offset = search(data, dllSize -((uint32_t) ea)-1, search_pattern, search_size, ((uint32_t) ea)+1);

        if (pattern_offset == -1)
            break;

        ea = (uint32_t *) (pattern_offset + search_head);
        found++;

        if (found != search_idx)
            continue;

        return (uint32_t *) ((int64_t) data + (int64_t) ea);
    }

    return NULL;
}

uint32_t *find_binary(char *data, DWORD dllSize, const char *search_pattern, uint32_t search_size, uint32_t search_head, int8_t search_idx)
{
    return _find_binary_ex(data, dllSize, search_pattern, search_size, search_head, search_idx, false);
}

uint32_t *find_binary_wildcard(char *data, DWORD dllSize, const char *search_pattern, uint32_t search_size, uint32_t search_head, int8_t search_idx)
{
    return _find_binary_ex(data, dllSize, search_pattern, search_size, search_head, search_idx, true);
}

uint32_t *find_binary_xref(char *data, DWORD dllSize, const char *search_pattern, uint32_t search_size, uint32_t search_head, int8_t search_idx, int8_t xref_search_idx)
{
    uint32_t* ea = find_binary(data, dllSize, search_pattern, search_size, search_head, search_idx);

    if (ea == NULL)
        return NULL;

    char *as_hex = (char *) &ea;
    ea = find_binary(data, dllSize, as_hex, 4, 0, xref_search_idx);

    return ea;
}

uint32_t get_table_size_by_xref(uint32_t *ea, uint32_t entry_size)
{
    uint32_t *orig_ea = ea;
    // Skip 10 entries because why not. We're looking for the end anyway
    ea = (uint32_t*)((uint32_t)ea+(entry_size*10));

    //fprintf(stderr, "orig ea %p, entry size %u, ea %p\n", orig_ea, entry_size, ea);
    bool found = false;

    while (!found)
    {
        uint32_t *as_int = (uint32_t*)&ea;

        if ( bst_search(g_xref_bst, *as_int) != NULL )
        {
            //fprintf(stderr, "found .text XREF in BST\n");
            found = true;
        }
        else if ( bst_search(g_data_xref_bst, *as_int) != NULL )
        {
            //fprintf(stderr, "found .data XREF in BST, data %0X\n", *as_int);
            found = true;
        }

        if (!found)
            ea = (uint32_t*)((uint32_t)ea+entry_size);
    }

    //fprintf(stderr, "found value %u (ea = %p, orig_ea = %p, entry size = %u)\n", ((((uint32_t)ea)-((uint32_t)orig_ea))/entry_size), ea, orig_ea, entry_size);
    return (((uint32_t)ea)-((uint32_t)orig_ea))/entry_size;
}

void print_cb(x86_insn_t *insn, void *arg)
{
    char line[LINE_SIZE];    /* buffer of line to print */
    x86_format_insn(insn, line, LINE_SIZE, intel_syntax);
    fprintf(stderr,"%s\n", line);
}

static void add_xref_to_bst(bst_t **bst, uint32_t addr, offset_list_t *extra_data)
{
    *bst = bst_insert_ex(*bst, addr, (void *)extra_data, &add_to_offset_list);
}

offset_list_t *make_new(x86_insn_t *insn)
{
    offset_list_t *res = (offset_list_t *)malloc(sizeof(offset_list_t));
    if (res == NULL)
        return NULL;

    res->offset = insn->offset;
    res->next = NULL;
    res->last = res;
    return res;
}

void add_xref(x86_insn_t *insn, void *arg)
{
    uint32_t addr = x86_get_address(insn);
    if ( addr > 0x5 )
    {
        offset_list_t *extra_data = make_new(insn);
        add_xref_to_bst(&g_xref_bst, addr, extra_data);
    }

    x86_op_t *op = x86_get_imm(insn);
    if ( op != NULL )
    {
        offset_list_t *extra_data = make_new(insn);
        add_xref_to_bst(&g_xref_bst, op->data.dword, extra_data);
    }

    // doesn't have an address directly
    op = x86_operand_1st(insn);
    if ( op != NULL && op->type == op_expression )
    {
        x86_ea_t exp = op->data.expression;
        offset_list_t *extra_data = make_new(insn);
        add_xref_to_bst(&g_xref_bst, exp.disp, extra_data);
    }

    op = x86_operand_2nd(insn);
    if ( op != NULL && op->type == op_expression )
    {
        x86_ea_t exp = op->data.expression;
        offset_list_t *extra_data = make_new(insn);
        add_xref_to_bst(&g_xref_bst, exp.disp, extra_data);
    }

    op = x86_operand_3rd(insn);
    if ( op != NULL && op->type == op_expression )
    {
        x86_ea_t exp = op->data.expression;
        offset_list_t *extra_data = make_new(insn);
        add_xref_to_bst(&g_xref_bst, exp.disp, extra_data);
    }
}

/* filtrer against known bad offsets and known bad instruction then write the disassembly in instruction_str and returns the offset containing the search value as hex */
static int32_t parse_instruction(char *buf, uint32_t buf_size, int32_t reloc_delta, uint32_t offset, uint32_t search_value, char *instruction_str)
{
    char *as_hex = (char*)&search_value;
    int32_t pattern_offset = search(buf+offset, 0x10, as_hex, 4, 0);
    if (pattern_offset == -1)
    {
        //LOG("Couldnt find address in instruction, skipping...\n");
        return -1;
        /* fallback to 2 bytes: not needed so far */
        /* if ( search_value <= 0xFFFF )
            pattern_offset = search(buf+offset, 0x10, as_hex, 2, 0);

            if ( pattern_offset == -1 )
            {
                LOG("Couldnt find address in instruction, skipping... (should not happen)\n");
                return -1;
            }
        */
    }

    if ( bst_search(g_bad_offset_bst, offset+(uint32_t)buf+pattern_offset+reloc_delta) != NULL )
    {
        LOG("skipping known bad offset\n");
        return -1;
    }

    x86_insn_t insn;
    int size = x86_disasm((unsigned char*)buf, buf_size, 0, offset, &insn);

    if ( size )
    {
        x86_op_t *op = x86_operand_1st(&insn);
        x86_op_t *op2 = x86_operand_2nd(&insn);

        if ( op != NULL && op->type == op_expression && op->data.expression.disp == (int32_t)search_value && strcmp(op->data.expression.base.name, "esp") == 0 )
        {
            x86_oplist_free(&insn);
            return -1;
        }
        if ( op2 != NULL && op2->type == op_expression && op2->data.expression.disp == (int32_t)search_value && strcmp(op2->data.expression.base.name, "esp") == 0 )
        {
            x86_oplist_free(&insn);
            return -1;
        }

        /* print instruction */
        x86_format_insn(&insn, instruction_str, LINE_SIZE, intel_syntax);
        x86_oplist_free(&insn);
    }

    return pattern_offset;
}

static uint32_t get_previous_lea_ebx(char *buf, uint32_t buf_size, uint32_t offset, uint32_t* value)
{
    x86_insn_t insn;
    int delta = 1;

    //TODO find a better way to go back in instructions... this might fail if we're unlucky
    while(true)
    {
        int size = x86_disasm((unsigned char*)buf, buf_size, 0, offset-delta, &insn);
        if ( size != 0 && insn.type == insn_mov && strcmp(insn.mnemonic, "lea")==0 )
        {
            x86_op_t *op = x86_operand_2nd(&insn);
            if ( op != NULL && op->type == op_expression )
            {
                x86_ea_t exp = op->data.expression;
                if (strcmp(exp.base.name, "ebx") == 0)
                {
                    /* FOUND! */
                    *value = exp.disp;
                    break;
                }
            }
        }
        x86_oplist_free(&insn);
        delta++;
    }

    x86_oplist_free(&insn);
    return (uint32_t)buf+offset-delta;
}

static uint32_t get_previous_push(char *buf, uint32_t buf_size, uint32_t offset, uint32_t* push_value)
{
    x86_insn_t insn;
    int delta = 1;

    //TODO find a better way to go back in instructions... this might fail if we're unlucky
    while(true)
    {
        int size = x86_disasm((unsigned char*)buf, buf_size, 0, offset-delta, &insn);
        if ( size != 0 && insn.type == insn_push )
        {
            /* FOUND! */
            break;
        }
        x86_oplist_free(&insn);
        delta++;
    }

    x86_op_t *op = x86_operand_1st(&insn);
    if (op->datatype == op_word)
    {
        *push_value = op->data.sword;
    }
    else
    {
        *push_value = op->data.sdword;
    }

    x86_oplist_free(&insn);
    return (uint32_t)buf+offset-delta;
}

static uint32_t get_next_call(char *buf, uint32_t buf_size, uint32_t offset, uint32_t* func_ea)
{
    x86_insn_t insn;
    int delta = 0;
    while(true)
    {
        int size = x86_disasm((unsigned char*)buf, buf_size, 0, offset+delta, &insn);
        if (insn.type == insn_call)
        {
            /* FOUND! */
            break;
        }
        x86_oplist_free(&insn);
        delta+=size;
    }

    x86_op_t *op = x86_operand_1st(&insn);
    if ( op->type == op_relative_far )
    {
        if (op->datatype == op_word)
        {
            *func_ea = (uint32_t)buf + op->data.sword + insn.addr + insn.size;
        }
        else
        {
            *func_ea = (uint32_t)buf + op->data.sdword + insn.addr + insn.size;
        }
    }

    x86_oplist_free(&insn);
    return (uint32_t)buf+offset+delta;
}

bool find_weird_update_patches(char *buf, uint32_t buf_size, uint32_t music_limit, update_patches_t *out)
{
    /* There are 4 patches to find with this routine, we assume "out" has room to store them */

    bool new_popn = false;
    if (music_limit > 2040)
        new_popn = true;

    uint32_t* ea;
    if ( new_popn )
        ea = find_binary(buf, buf_size, "\x83\xC4\x04\x89\x44\x24\x14\xC7\x44\x24", 10, 0, 0);
    else
        ea = find_binary(buf, buf_size, "\x83\xC4\x04\x3B\xC5\x74\x09", 7, 0, 0);

    /* part1: find "push" patch */
    uint32_t push_value = 0;
    uint32_t push_ea = get_previous_push(buf, buf_size, (uint32_t)ea-(uint32_t)buf, &push_value);

    out[3] = { .idx = MUSIC_IDX, .patch_method = 11, .value = push_value, .offset = push_ea};

    uint32_t func_ea = 0;
    (void)get_next_call(buf, buf_size, (uint32_t)ea-(uint32_t)buf, &func_ea);

    /* seek end of func_ea */
    uint32_t *func_ea_end = find_binary((char *)func_ea, 0x10000, "\xCC\xCC\xCC\xCC\xCC", 5, 0, 0);

    /* part2: now we need to find 3 "lea ?, [ebx+?????]" instructions at the end of func_ea */
    uint32_t lea_value = 0;
    uint32_t lea_ea = (uint32_t)func_ea_end;
    for (uint8_t i = 0; i < 3; i++)
    {
        if ( new_popn ) //in newer games we have to skip every other match
            lea_ea = get_previous_lea_ebx(buf, buf_size, (uint32_t)lea_ea-(uint32_t)buf, &lea_value);

        lea_ea = get_previous_lea_ebx(buf, buf_size, (uint32_t)lea_ea-(uint32_t)buf, &lea_value);
        out[2-i] = { .idx = MUSIC_IDX, .patch_method = (uint8_t)(11-i), .value = lea_value, .offset = lea_ea};
    }

    return true;
}

int load_offset_blacklist_from_file(){
    int res = 0;
    FILE *file = fopen("data_mods\\patches_blacklist.dat", "rb");

    if ( file == NULL )
    {
        return 0;
    }

    char line[32];
    while (fgets(line, sizeof(line), file)) {
        uint32_t offset = strtol(line, NULL, 16);
        if ( offset != 0 && bst_search(g_bad_offset_bst, offset) == NULL )
        {
            res++;
            add_xref_to_bst(&g_bad_offset_bst, offset, NULL);
        }
    }
    fclose(file);

    return res;
}

bool make_omnimix_patch(const char *dllFilename, membuf_t *membuf, char *datecode){
    DWORD dllSize = 0;
    char *buf = getDllData(dllFilename, &dllSize);

    char *text = NULL;
    DWORD text_size = 0;

    char *data = NULL;
    DWORD data_size = 0;

    PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS)(buf + ((PIMAGE_DOS_HEADER)buf)->e_lfanew);
    PIMAGE_SECTION_HEADER section_header = IMAGE_FIRST_SECTION(headers);

    uint32_t buf_base_addr = (uint32_t)(headers->OptionalHeader.ImageBase);
    int32_t buf_reloc_delta = buf_base_addr - (uint32_t)buf;
    uint32_t text_base_addr = 0;

    for (WORD SectionIndex = 0; SectionIndex < headers->FileHeader.NumberOfSections; SectionIndex++)
    {
        PIMAGE_SECTION_HEADER curr_header = &section_header[SectionIndex];

        if ( strcmp( (const char*)curr_header->Name, ".text" ) == 0 || curr_header->Characteristics & IMAGE_SCN_CNT_CODE )
        {
            text = (char*) ((int64_t)buf + curr_header->VirtualAddress);
            text_base_addr = buf_base_addr + curr_header->VirtualAddress;
            text_size = curr_header->Misc.VirtualSize;
        }
        if ( strcmp( (const char*)curr_header->Name, ".data" ) == 0 )
        {
            data = (char*) ((int64_t)buf + curr_header->VirtualAddress);
            data_size = curr_header->Misc.VirtualSize;
        }
    }

    x86_init(opt_none, NULL, NULL);
    // disassemble .text section and add all xrefs to BST
    LOG("popnhax: patch_db_gen: Disassembling .text section... ");
    unsigned int num_insn = x86_disasm_range( (unsigned char *)text, (uint32_t) buf, 0, text_size, &add_xref, NULL);
    LOG("done (%u instructions)\n", num_insn);

    //add all data xrefs to BST (take anything which looks like an address in .data section (eg. 0x10******) and is at an offset =0%4.. experimental)
    {
        uint8_t search_byte = ((uint8_t*)&buf)[3]; //retrieve highest byte from base address to account for relocations
        LOG("popnhax: patch_db_gen: Parsing .data section... ");
        uint32_t *data_ea = 0;
        while (true)
        {
            int64_t pattern_offset = search(data, data_size -((uint32_t) data_ea)-1, (const char *)&search_byte, 1, ((uint32_t) data_ea)+1);
            if (pattern_offset == -1)
                break;

            data_ea = (uint32_t *) ((uint32_t)data + pattern_offset - 3);
            uint32_t *as_int = (uint32_t*)data_ea;

            if (((((uint32_t)data_ea)&3) == 0))
                add_xref_to_bst(&g_data_xref_bst, *as_int, NULL);

            data_ea = (uint32_t *) (pattern_offset);
        }
        LOG("done\n");
    }

    //make bad offset bst
    LOG("popnhax: patch_db_gen: Loading offset blacklist... ");
    for (uint8_t i = 0; i < sizeof(g_offset_blacklist)/sizeof(uint32_t); i++)
    {
        add_xref_to_bst(&g_bad_offset_bst, g_offset_blacklist[i], NULL);
    }

    int bad_offset_count = load_offset_blacklist_from_file();
    LOG("done");
    if ( bad_offset_count > 0 )
        LOG(" (%d additional bad offsets found in data_mods\\patches_blacklist.dat)", bad_offset_count);
    LOG("\n");

    LOG("popnhax: patch_db_gen: Generating patches\n");

    membuf_printf(membuf, "<?xml version='1.0' encoding='shift-jis'?>\n");
    membuf_printf(membuf, "<patches target=\"%s\">\n", datecode);

    /* init tables */
    g_tables[MUSIC_IDX].name = strdup("music");
    g_tables[CHART_IDX].name = strdup("chart");
    g_tables[STYLE_IDX].name = strdup("style");
    g_tables[FLAVOR_IDX].name = strdup("flavor");
    g_tables[CHARA_IDX].name = strdup("chara");

    LOG("popnhax: patch_db_gen:     buffer_base_addrs... ");
    // These all reference the first entry in their respective tables
    g_tables[MUSIC_IDX].addr = find_binary_xref(buf, dllSize, "\x00\x83\x7C\x83\x62\x83\x76\x83\x58\x00", 10, 1, 0, 0);
    g_tables[CHART_IDX].addr = find_binary_xref(buf, dllSize, "\x00\x70\x6F\x70\x6E\x31\x00\x00", 8, 1, 0, 1);
    g_tables[STYLE_IDX].addr = find_binary(buf, dllSize, "\x01\x00\x00\x00\xFF\x54\x0C\x00\x1A\x00\x00\x00\x11\x00\x00\x00", 16, 0, 2);
    g_tables[FLAVOR_IDX].addr = find_binary(buf, dllSize, "\x00\x82\xBB\x82\xEA\x82\xA2\x82\xAF\x81\x5B\x00\x00\x00\x82\xA4\x82", 17, 1, 0);
    g_tables[CHARA_IDX].addr = find_binary_xref(buf, dllSize, "\x00\x62\x61\x6D\x62\x5F\x31\x61\x00", 9, 1, 0, 0);
    LOG("done\n");

    // Modify the entry sizes as required
    g_tables[MUSIC_IDX].size = 0xAC;
    g_tables[CHART_IDX].size = 0x20; // Probably won't change?
    g_tables[STYLE_IDX].size = 0x10; // Unlikely to change
    g_tables[FLAVOR_IDX].size = 0x60;
    g_tables[CHARA_IDX].size = 0x4C;

    LOG("popnhax: patch_db_gen:     limits... ");
    // buffer_addr + (buffer_entry_size * limit) should give you the very end of the array (after the last entry)
    g_tables[MUSIC_IDX].limit = get_table_size_by_xref(g_tables[MUSIC_IDX].addr, g_tables[MUSIC_IDX].size);
    g_tables[CHART_IDX].limit = get_table_size_by_xref(g_tables[CHART_IDX].addr, g_tables[CHART_IDX].size);
    g_tables[STYLE_IDX].limit = get_table_size_by_xref(g_tables[STYLE_IDX].addr, g_tables[STYLE_IDX].size);
    g_tables[FLAVOR_IDX].limit = get_table_size_by_xref(g_tables[FLAVOR_IDX].addr, g_tables[FLAVOR_IDX].size);
    g_tables[CHARA_IDX].limit = get_table_size_by_xref(g_tables[CHARA_IDX].addr, g_tables[CHARA_IDX].size);
    LOG("done\n");

    membuf_printf(membuf, "\t<limits>\n");

    for (int i=0; i < NUM_IDX; i++)
    {
        //patch_target, buffer_addr, entry_size = buffer_info
        membuf_printf(membuf, "\t\t<%s __type=\"u32\">%d</%s>\n", g_tables[i].name, g_tables[i].limit, g_tables[i].name);
    }
    membuf_printf(membuf, "\t</limits>\n");

    membuf_printf(membuf, "\t<buffer_base_addrs>\n");
    for (int i=0; i < NUM_IDX; i++)
    {
        //patch_target, buffer_addr, entry_size = buffer_info
        membuf_printf(membuf, "\t\t<%s __type=\"str\">0x%0x</%s>\n", g_tables[i].name, (uint32_t)(g_tables[i].addr)+buf_reloc_delta, g_tables[i].name);
    }
    membuf_printf(membuf, "\t</buffer_base_addrs>\n");

    LOG("popnhax: patch_db_gen:     buffers_patch_addrs... ");
    membuf_printf(membuf, "\t<buffers_patch_addrs>\n");
    for (int i=0; i < NUM_IDX; i++)
    {
        uint32_t search_value_base = (uint32_t)g_tables[i].addr;
        for (uint32_t j = search_value_base; j < (search_value_base + g_tables[i].size + 1); j++)
        {
            bst_t *bst = bst_search(g_xref_bst, j);
            if ( bst != NULL )
            {
                offset_list_t *curr_elem = (offset_list_t*)(bst->extra_data);
                while ( curr_elem )
                {
                    char line[LINE_SIZE];
                    line[0] = '\0';
                    int32_t addr_offset = parse_instruction(text, text_size, buf_reloc_delta, curr_elem->offset, j, line);

                    if (addr_offset != -1)
                    {
                        membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
                        membuf_printf(membuf, "\t\t<%s __type=\"str\">0x%0x</%s>\n\n", g_tables[i].name, (uint32_t)text_base_addr+(curr_elem->offset)+addr_offset, g_tables[i].name);
                    }
                    curr_elem = curr_elem->next;
                }
            }
        }
    }

    // This is a hack for Usaneko.
    // Usaneko's code is dumb.
    // If it doesn't find *this* address it won't stop the loop.
    {
        uint32_t random_lv7 = (uint32_t)find_binary_xref(buf, dllSize, "\x83\x89\x83\x93\x83\x5F\x83\x80\x20\x4C\x76\x20\x37\x00\x00\x00", 16, 0, 0, 0);
        bst_t *bst = bst_search(g_xref_bst, random_lv7);
        if ( bst != NULL )
        {
            offset_list_t *curr_elem = (offset_list_t*)(bst->extra_data);
            while ( curr_elem )
            {
                char line[LINE_SIZE];
                line[0] = '\0';
                int32_t addr_offset = parse_instruction(text, text_size, buf_reloc_delta, curr_elem->offset, random_lv7, line);

                if (addr_offset != -1)
                {
                    membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
                    membuf_printf(membuf, "\t\t<%s __type=\"str\">0x%0x</%s>\n\n", g_tables[MUSIC_IDX].name, (uint32_t)text_base_addr+(curr_elem->offset)+addr_offset, g_tables[MUSIC_IDX].name);
                }
                curr_elem = curr_elem->next;
            }
        }
    }
    LOG("done\n");
    membuf_printf(membuf, "\t</buffers_patch_addrs>\n");

    membuf_printf(membuf, "\t<other_patches>\n");
    /* create update_patches */
    update_patches_t update_patches[23] = {
        { .idx = MUSIC_IDX, .patch_method = 0, .value = g_tables[MUSIC_IDX].limit - 1 },
        { .idx = MUSIC_IDX, .patch_method = 0, .value = g_tables[MUSIC_IDX].limit },
        { .idx = CHART_IDX, .patch_method = 0, .value = g_tables[CHART_IDX].limit },
        { .idx = CHART_IDX, .patch_method = 0, .value = g_tables[CHART_IDX].limit - 1 },
        { .idx = CHARA_IDX, .patch_method = 0, .value = g_tables[CHARA_IDX].limit },
        { .idx = FLAVOR_IDX, .patch_method = 0, .value = g_tables[FLAVOR_IDX].limit - 1 },
        { .idx = FLAVOR_IDX, .patch_method = 0, .value = g_tables[FLAVOR_IDX].limit },

    /* These values may change in a future patch, but they worked for usaneko/peace/kaimei/unilab for now.
     * These could possibly be done using something similar to the find_weird_update_patches code. */
        { .idx = MUSIC_IDX, .patch_method = 1, .value = 0x1BD0 - (1780 - g_tables[MUSIC_IDX].limit) * 4 },
        { .idx = MUSIC_IDX, .patch_method = 1, .value = 0x1Bcf - (1780 - g_tables[MUSIC_IDX].limit) * 4 },
        { .idx = MUSIC_IDX, .patch_method = 2, .value = 0xA6E0 - (1780 - g_tables[MUSIC_IDX].limit) * 0x18 },
        { .idx = MUSIC_IDX, .patch_method = 3, .value = 0x29B7 - (1780 - g_tables[MUSIC_IDX].limit) * 6 },
        { .idx = MUSIC_IDX, .patch_method = 4, .value = 0x3E944 - (1780 - g_tables[MUSIC_IDX].limit) * 0x90 },

        { .idx = MUSIC_IDX, .patch_method = 4, .value = 0x3E948 - (1780 - g_tables[MUSIC_IDX].limit) * 0x90 },
        { .idx = MUSIC_IDX, .patch_method = 5, .value = 0x1F4F4 - (1780 - g_tables[MUSIC_IDX].limit) * 0x48 },
        { .idx = MUSIC_IDX, .patch_method = 5, .value = 0x1F4C0 - (1780 - g_tables[MUSIC_IDX].limit) * 0x48 },
        { .idx = MUSIC_IDX, .patch_method = 5, .value = 0x1F4F0 - (1780 - g_tables[MUSIC_IDX].limit) * 0x48 },
        { .idx = MUSIC_IDX, .patch_method = 6, .value = 0x7D3D8 - (1780 - g_tables[MUSIC_IDX].limit) * 0x120 },
        { .idx = MUSIC_IDX, .patch_method = 6, .value = 0x7D3D4 - (1780 - g_tables[MUSIC_IDX].limit) * 0x120 },
        { .idx = MUSIC_IDX, .patch_method = 7, .value = 0x1D8E58 - (1780 - g_tables[MUSIC_IDX].limit) * 0x440 },
        { .idx = MUSIC_IDX, .patch_method = 7, .value = 0x1D9188 - (1780 - g_tables[MUSIC_IDX].limit) * 0x440 },
        { .idx = MUSIC_IDX, .patch_method = 8, .value = 0x5370 - (1780 - g_tables[MUSIC_IDX].limit) * 0x0c },
        { .idx = FLAVOR_IDX, .patch_method = 8, .value = g_tables[FLAVOR_IDX].limit * 0x0c },
        { .idx = FLAVOR_IDX, .patch_method = 8, .value = g_tables[FLAVOR_IDX].limit * 0x0c + 4 },
    };

    LOG("popnhax: patch_db_gen:     other_patches... ");
    for (int i = 0; i < 23; i++)
    {
        uint32_t j = update_patches[i].value;
        bst_t *bst = bst_search(g_xref_bst, j);
        if ( bst != NULL )
        {
            offset_list_t *curr_elem = (offset_list_t*)(bst->extra_data);
            while ( curr_elem )
            {
                char line[LINE_SIZE];
                line[0] = '\0';
                int32_t addr_offset = parse_instruction(text, text_size, buf_reloc_delta, curr_elem->offset, j, line);

                if (addr_offset != -1)
                {
                    membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
                    membuf_printf(membuf, "\t\t<%s __type=\"str\" method=\"%d\" expected=\"0x%x\">0x%0x</%s>\n\n", g_tables[update_patches[i].idx].name, update_patches[i].patch_method, update_patches[i].value, (uint32_t)text_base_addr+(curr_elem->offset)+addr_offset, g_tables[update_patches[i].idx].name);
                }
                curr_elem = curr_elem->next;
            }
        }
    }

    update_patches_t weird_update_patches[4];
    find_weird_update_patches(buf, dllSize, g_tables[MUSIC_IDX].limit, weird_update_patches);
    for (int i = 0; i < 4; i++)
    {
        char line[LINE_SIZE];
        line[0] = '\0';
        int32_t addr_offset = parse_instruction(buf, dllSize, buf_reloc_delta, weird_update_patches[i].offset-(uint32_t)buf, weird_update_patches[i].value, line);

        if (addr_offset != -1)
        {
            membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
            membuf_printf(membuf, "\t\t<%s __type=\"str\" method=\"%d\" expected=\"0x%x\">0x%0x</%s>\n\n", g_tables[weird_update_patches[i].idx].name, weird_update_patches[i].patch_method, weird_update_patches[i].value, weird_update_patches[i].offset+addr_offset+buf_reloc_delta, g_tables[weird_update_patches[i].idx].name);
        }
    }
    LOG("done\n");
    membuf_printf(membuf, "\t</other_patches>\n");

    uint32_t *hook_addrs[2] = { find_binary_wildcard(buf, dllSize, "\x8B\xC6\xE8????\x83\xF8?\x7D?\x56\x8A\xC3\xE8????\x83\xC4\x04\x3D????\x7D?", 30, 0, 0),
                                find_binary_wildcard(buf, dllSize, "\x83\xF8?\x0F\x9C\xC0\xE8", 7, 0, 0)};

    LOG("popnhax: patch_db_gen:     hook_addrs... ");
    membuf_printf(membuf, "\t<hook_addrs>\n");
    for (int i=0; i<2; i++)
    {
        if (hook_addrs[i] == NULL)
            continue;

        char line[LINE_SIZE];
        x86_insn_t insn;

        int delta = 0;
        if (i == 1)
        {
            //hook offset is actually 2 instructions later in this case
            delta = x86_disasm((unsigned char*)buf, dllSize, 0, ((uint32_t)(hook_addrs[i])-(uint32_t)buf), &insn);
            x86_oplist_free(&insn);
            delta += x86_disasm((unsigned char*)buf, dllSize, 0, ((uint32_t)(hook_addrs[i])-(uint32_t)buf+delta), &insn);
            x86_oplist_free(&insn);
        }

        int size = x86_disasm((unsigned char*)buf, dllSize, 0, ((uint32_t)(hook_addrs[i])-(uint32_t)buf+delta), &insn);
        if ( size ) {
            /* print instruction */
            x86_format_insn(&insn, line, LINE_SIZE, intel_syntax);
            membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
            x86_oplist_free(&insn);
        }

        membuf_printf(membuf, "\t\t<offset __type=\"str\" method=\"%d\">0x%x</offset>\n\n", i, (uint32_t)hook_addrs[i]+delta+buf_reloc_delta);
    }
    LOG("done\n");

    membuf_printf(membuf, "\t</hook_addrs>\n");

    membuf_printf(membuf, "</patches>\n");

    LOG("popnhax: patch_db_gen: Patch data generated\n");

    x86_cleanup();

    return true;
}