#include <cstdio>
#include <cstring>
#include <io.h>
#include <windows.h>

#include "util/log.h"
#include "util/patch.h"

#include "tableinfo.h"
#include "loader.h"

#define MAX_REPLACE_SIZE 16384

FILE *g_dict_applied_fp;
bool patch_sjis(const char *dllFilename, const char *find, uint8_t find_size, int64_t *offset, char *replace, uint8_t replace_size, bool dump_dict) {
    static DWORD dllSize = 0;
    static char *data = getDllData(dllFilename, &dllSize);
    static uint8_t first_offset = 0;

    int64_t offset_orig = *offset;
    uint64_t patch_addr;
    bool valid_sjis = false;
    do {
        *offset = _search(data, dllSize-*offset, find, find_size, *offset);
        if (*offset == -1) {
            *offset = offset_orig;
            return false;
        }

        if ( !first_offset )
        {
            if (dump_dict)
            {
                LOG("popnhax: dump_dict: dump applied patches in dict_applied.txt\n");
                g_dict_applied_fp = fopen("dict_applied.txt","wb");
            }
            first_offset = 1;
        }

        patch_addr = (int64_t)data + *offset;

        /* filter out partial matches (check that there isn't a valid sjis char before our match) */
        uint8_t byte1 = *(uint8_t*)(patch_addr-2);
        uint8_t byte2 = *(uint8_t*)(patch_addr-1);
        bool valid_first  = ((0x81 <= byte1) && (byte1 <= 0x9F)) || ((0xE0 <= byte1) && (byte1 <= 0xFC));
        bool valid_secnd  = ((0x40 <= byte2) && (byte2 <= 0x9E)) || ((0x9F <= byte2) && (byte2 <= 0xFC));
        valid_sjis = valid_first && valid_secnd;

        if (valid_sjis)
        {
            printf("Partial match at offset 0x%x, retry...\n",(uint32_t)*offset);
            *offset += find_size;
        }
    } while (valid_sjis);

    if (dump_dict)
    {
        //fprintf(g_dict_applied_fp,"0x%x;%s;%s\n",rva_to_offset(dllFilename, (uint32_t)*offset),(char*)find,(char*)replace);
        fprintf(g_dict_applied_fp,";%s;%s\n",(char*)find,(char*)replace);
    }
    /* safety check replace is not too big */
    uint8_t free_size = find_size-1;
    do
    {
        free_size++;
    }
    while ( *(uint8_t *)(patch_addr+free_size) == 0 );

    if ((free_size-1) < replace_size)
    {
        LOG("WARNING: translation %s is too big, truncating to ",(char *)replace);
        replace_size = free_size-1;
        replace[replace_size-1] = '\0';
        LOG("%s\n",(char *)replace);
    }

    patch_memory(patch_addr, (char *)replace, replace_size);

    return true;
}

#define RELOC_HIGHLOW 0x3
static void perform_reloc(char *data, int32_t delta, uint32_t ext_base, uint32_t ext_delta)
{
    PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS)((int64_t)data + ((PIMAGE_DOS_HEADER)data)->e_lfanew);
    PIMAGE_DATA_DIRECTORY datadir = &headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(data + datadir->VirtualAddress);

    while(reloc->VirtualAddress != 0)
    {
        if (reloc->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
        {
            DWORD relocDescNb = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            LPWORD relocDescList = (LPWORD)((LPBYTE)reloc + sizeof(IMAGE_BASE_RELOCATION));

            for (DWORD i = 0; i < relocDescNb; i++)
            {
                if ( ((relocDescList[i])>>12) == RELOC_HIGHLOW )
                {
                    DWORD_PTR *p = (DWORD_PTR *)( data + (reloc->VirtualAddress + ((relocDescList[i])&0x0FFF)) );
                    /* Change the offset to adapt to injected module base address */

                    DWORD old_prot;
                    VirtualProtect((LPVOID)p, 4, PAGE_EXECUTE_READWRITE, &old_prot);
                    *p += delta;
                    if ( ext_base > 0 && *p >= ((int64_t)data+ext_base) )
                    {
                        //fprintf(stderr,"reloc rva %lx to ext ", *p);
                        *p += ext_delta;
                        //fprintf(stderr," %lx\n", *p);
                    }
                    VirtualProtect((LPVOID)p, 4, old_prot, &old_prot);
                }
            }
        }
        /* Set reloc pointer to the next relocation block */
        reloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)reloc + reloc->SizeOfBlock);
    }
}

#define BYTE3_TO_UINT(bp) \
     (((unsigned int)(bp)[0] << 16) & 0x00FF0000) | \
     (((unsigned int)(bp)[1] << 8) & 0x0000FF00) | \
     ((unsigned int)(bp)[2] & 0x000000FF)

#define BYTE2_TO_UINT(bp) \
    (((unsigned int)(bp)[0] << 8) & 0xFF00) | \
    ((unsigned int) (bp)[1] & 0x00FF)

#define IPS_EOF 0x464F45

static bool patch_translation_ips(const char *dllFilename, const char *foldername, bool dump_dll)
{
#define IPS_READ(_ips_read_dest, _ips_read_size, _ips_read_name) do {\
    if ( fread(_ips_read_dest, 1, _ips_read_size, ips_fp) != _ips_read_size )\
        {\
            LOG("CANNOT READ %s\n", _ips_read_name);\
            return false;\
        }\
} while (0)
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

    char dict_filepath[64];
    sprintf(dict_filepath, "%s%s%s", "data_mods\\", foldername, "\\popn22.ips");
    FILE *ips_fp = fopen(dict_filepath, "rb");

    if (ips_fp == NULL)
    {
        return false;
    }

    LOG("popnhax: translation: popn22.ips found\n");

    /* check .ips header */
    uint8_t buffer[8];
    if (fread(&buffer, 1, 5, ips_fp) != 5)
        return false;

    if (memcmp(buffer, "PATCH", 5) != 0)
    {
        LOG("popnhax: translation: invalid .ips header\n");
        return false;
    }

    if (dump_dll)
    {
        LOG("popnhax: translation debug: dump dll before patch\n");
        FILE* dllrtp = fopen("dllruntime.dll", "wb");
        fwrite(data, 1, dllSize, dllrtp);
        fclose(dllrtp);
        LOG("popnhax: translation debug: dllruntime.dll generated\n");
    }

    /* undo all relocation so you can apply ips patch with correct values, we'll reapply them later */
    PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS)((int64_t)data + ((PIMAGE_DOS_HEADER)data)->e_lfanew);
    DWORD_PTR reloc_delta = (DWORD_PTR)((int64_t)data - headers->OptionalHeader.ImageBase);
    perform_reloc(data, -1*reloc_delta, 0, 0);

    uint32_t trans_base = 0; /* eclale patch adds new section header which I'm relocating */
    uint32_t trans_base_offset = 0;
    uint32_t trans_rebase = 0;

    uint32_t offset = 0;
    uint16_t size = 0;
    uint16_t count = 0;
    uint8_t  replace[MAX_REPLACE_SIZE] = {0};

    while ( fread(&offset, 1, 3, ips_fp) == 3 && offset != IPS_EOF )
    {
        bool skip = false;

        /* need to convert offset to rva before applying patch since the dll is already in memory */
        uint8_t *bp = (uint8_t *)&offset;
        offset = BYTE3_TO_UINT(bp);
        uint32_t rva;

        if (offset < 0x400)
        {
            rva = offset;
        }
        else if ( !offset_to_rva(dllFilename, offset, &rva) )
        {
            LOG("Invalid offset %x conversion. Skip this patch\n", offset);
            skip = true;
            /* still need to go through the loop to increase read pointer accordingly */
        }

        IPS_READ(&size, 2, "SIZE");
        bp = (uint8_t *)&size;
        size = BYTE2_TO_UINT(bp);
        ++count;

        //LOG("%03d: offset %x converted to %x\nsize %d\n", count, offset, rva,size);
        if ( size == 0 )
        {
            uint8_t value;

            IPS_READ(&size, 2, "RLE SIZE");
            bp = (uint8_t *)&size;
            size = BYTE2_TO_UINT(bp);

            IPS_READ(&value, 1, "RLE VALUE");

            //LOG("RLE PATCH! size %d value %d\n", size, value);
            //fprintf(stderr, "rle value %d (%d bytes)\n",value, size);
            if ( size > MAX_REPLACE_SIZE )
            {
                LOG("RLE replacement too big.\n");
                return false;
            }
            memset(replace, value, size);
        }
        else
        {
            if ( size > MAX_REPLACE_SIZE )
            {

                uint16_t remaining = size;
                uint32_t chunk_rva = rva;

                do {
                    /* patch in multiple chunks */
                    LOG("multipart patch: rva %x, %d remaining\n", chunk_rva, remaining);
                    IPS_READ(&replace, MAX_REPLACE_SIZE, "DATA");

                    if ( !skip )
                        patch_memory((int64_t)data+chunk_rva, (char *)replace, MAX_REPLACE_SIZE);

                    remaining -= MAX_REPLACE_SIZE;
                    chunk_rva += MAX_REPLACE_SIZE;

                } while (remaining > MAX_REPLACE_SIZE);

                size = remaining;
                rva = chunk_rva;

            }

            IPS_READ(&replace, size, "DATA");
        }

        /* eclale woes */
        if ( trans_base == 0 && rva < 0x400 )
        {
            if (memcmp(replace, ".trans", 6) == 0)
            {
                    trans_base = *(uint32_t*)(replace+0x0C);
                    trans_base_offset = *(uint32_t*)(replace+0x14);
                    //LOG("found .trans section at offset %x rva %x\n", trans_base_offset, trans_base);
            }
        }

        if ( trans_base_offset != 0 && offset >= trans_base_offset )
        {
            /* patching into new section */
            if ( trans_rebase == 0 )
            {
                HANDLE hProc = GetCurrentProcess();
                LPVOID myAlloc = VirtualAllocEx(hProc, NULL, 16384, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                if (myAlloc == NULL)
                {
                    LOG("Failed to allocate memory in target process. Error: 0x%lX\n", GetLastError());
                    exit(0);
                }
                trans_rebase = (uint32_t)myAlloc - (uint32_t)data;
                //LOG( "virtualalloc worked; address %x (%p)\n", trans_rebase, myAlloc);

                /* seems useless */
                //memcpy(replace, &trans_rebase, 4);
                //patch_memory((int64_t)data+0x02d4, (char *)replace, 4);
                //LOG( "patched .trans section address to %02x %02x %02x %02x\n", replace[0], replace[1], replace[2], replace[3]);
            }
            rva = (offset - trans_base_offset) + trans_rebase;
            //LOG( "off %x - base %x + rebase %x = %x\n\n", offset, trans_base_offset, trans_rebase, rva);
            //LOG( "offset %x relocated to rva %x.", offset, rva);
        }

        if ( !skip )
        {
            patch_memory((int64_t)data+rva, (char *)replace, size);
        }
    }

    /* redo all relocation now the dll is patched */
    perform_reloc(data, reloc_delta, trans_base, trans_rebase-trans_base);

    LOG("popnhax: translation: IPS patch applied.\n");

    if (dump_dll)
    {
        LOG("popnhax: translation debug: dump dll after patch\n");
        FILE* dllrtp = fopen("dllruntime_patched.dll", "wb");
        fwrite(data, 1, dllSize, dllrtp);
        fclose(dllrtp);
    }

    fclose(ips_fp);
    return true;
#undef IPS_READ
}

static bool patch_translation_dict(const char *dllFilename, const char *foldername, bool dump_dict)
{
    char original[128];
    char translation[128];
    uint8_t buffer;
    int64_t curr_offset = 0;
    uint8_t word_count = 0;
    uint8_t orig_size = 0;

    char dict_filepath[64];
    sprintf(dict_filepath, "%s%s%s", "data_mods\\", foldername, "\\popn22.dict");
    FILE *dict_fp = fopen(dict_filepath, "rb");

    if (dict_fp == NULL)
    {
        return false;
    }

    LOG("popnhax: translation: popn22.dict file found\n");

#define STATE_WAITING 0
#define STATE_ORIGINAL 1
#define STATE_TRANSLATION 2
    uint16_t err_count = 0;
    uint8_t state = STATE_WAITING;
    uint8_t arr_idx = 0;
    while (fread(&buffer, 1, 1, dict_fp) == 1)
    {
        switch (state)
        {
            case STATE_WAITING:
                if (buffer == ';')
                {
                    state = STATE_ORIGINAL;
                    arr_idx = 0;
                }
                else
                {
                    LOG("Unexpected char %c\n", buffer);
                    return false;
                }
                break;
            case STATE_ORIGINAL:
                if (buffer == ';')
                {
                    original[arr_idx++] = '\0';
                    state = STATE_TRANSLATION;
                    orig_size = arr_idx;
                    arr_idx = 0;
                }
                else
                {
                    original[arr_idx++] = buffer;
                }
                break;
            case STATE_TRANSLATION:
                if (buffer == ';')
                {
                    /* end of word, let's patch! */
                    translation[arr_idx-1] = '\0'; /* strip last \n */
                    while ( arr_idx < orig_size )
                    {
                        translation[arr_idx++] = '\0'; /* fill with null when translation is shorter */
                    }
                    printf("%d: %s -> %s\n",++word_count,(char *)original,(char *)translation);

                    /* patch all occurrences */
                    /*curr_offset = 0;
                    uint8_t count = 0;
                    while (patch_sjis(dllFilename, original, orig_size, &curr_offset, translation, arr_idx-1, dump_dict))
                    {
                     count++;
                    }
                    LOG("%d occurrences found\n",count);
                    */
                    if (!patch_sjis(dllFilename, original, orig_size, &curr_offset, translation, arr_idx-1, dump_dict))
                    {
                        printf("Warning: string %s (%s) not found in order, trying again.\n", (char *)original, (char *)translation);
                        curr_offset = 0;
                        if (!patch_sjis(dllFilename, original, orig_size, &curr_offset, translation, arr_idx-1, dump_dict))
                        {
                            LOG("Warning: string %s not found, skipping.\n", (char *)original);
                            err_count++;
                        }
                    }

                    state = STATE_ORIGINAL;
                    arr_idx = 0;
                }
                else
                {
                    translation[arr_idx++] = buffer;
                }
                break;
            default:
                break;
        }
    }
    LOG("popnhax: translation: patched %u strings", word_count - err_count);
    if (err_count)
        LOG(" (%u skipped strings)", err_count);
    LOG("\n");

    if (dump_dict)
        fclose(g_dict_applied_fp);
    return true;
#undef STATE_WAITING
#undef STATE_ORIGINAL
#undef STATE_TRANSLATION
}

bool patch_translate(const char *dllFilename, const char *folder, bool debug)
{
    bool ips_done = false;
    bool dict_done = false;

    ips_done = patch_translation_ips(dllFilename, folder, debug);
    dict_done = patch_translation_dict(dllFilename, folder, debug);

    return ips_done || dict_done;
}