#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define MAX_FILE_LINES 20000
#define BASE_PATH "[media]/"

typedef struct
{
	char lines[MAX_FILE_LINES][MAX_LINE];
	int count;
} FileBuffer;

/*
 * 不要在各个 patch 函数里再定义 FileBuffer fb;
 * 只能保留这个全局静态缓冲区，否则会栈溢出。
 */
static FileBuffer g_fb;

int contains(const char* line, const char* key)
{
	return NULL != strstr(line, key);
}

int line_equals_trimmed(const char* line, const char* target)
{
	const char* start;
	const char* end;
	size_t len_target;

	start = line;
	while (('\t' == *start) || (' ' == *start) || ('\r' == *start) || ('\n' == *start))
		start++;

	end = start + strlen(start);
	while ((start < end) && (('\t' == *(end - 1)) || (' ' == *(end - 1)) || ('\r' == *(end - 1)) || ('\n' == *(end - 1))))
		end--;

	len_target = strlen(target);
	if ((size_t)(end - start) != len_target)
		return 0;

	return 0 == strncmp(start, target, len_target);
}

int read_file(const char* path, FileBuffer* fb)
{
	FILE* fp;

	fp = fopen(path, "r");
	if (NULL == fp)
	{
		printf("[ERROR] cannot open: %s\n", path);
		return 0;
	}

	fb->count = 0;
	while ((fb->count < MAX_FILE_LINES) && (NULL != fgets(fb->lines[fb->count], MAX_LINE, fp)))
		fb->count++;

	fclose(fp);

	if (MAX_FILE_LINES <= fb->count)
		printf("[WARN] line limit reached while reading: %s\n", path);

	return 1;
}

int write_file(const char* path, const FileBuffer* fb)
{
	FILE* fp;
	int i;

	fp = fopen(path, "w");
	if (NULL == fp)
	{
		printf("[ERROR] cannot write: %s\n", path);
		return 0;
	}

	for (i = 0; i < fb->count; i++)
		fputs(fb->lines[i], fp);

	fclose(fp);
	return 1;
}

int insert_line(FileBuffer* fb, int pos, const char* text)
{
	int i;
	size_t len;

	if ((0 > pos) || (fb->count < pos) || (MAX_FILE_LINES <= fb->count))
		return 0;

	len = strlen(text);
	if (MAX_LINE <= len)
		return 0;

	for (i = fb->count; i > pos; i--)
		strcpy(fb->lines[i], fb->lines[i - 1]);

	strcpy(fb->lines[pos], text);
	fb->count++;
	return 1;
}

int delete_line(FileBuffer* fb, int pos)
{
	int i;

	if ((0 > pos) || (fb->count <= pos))
		return 0;

	for (i = pos; i < fb->count - 1; i++)
		strcpy(fb->lines[i], fb->lines[i + 1]);

	fb->count--;
	return 1;
}

int find_line_contains(const FileBuffer* fb, const char* key, int start)
{
	int i;

	for (i = start; i < fb->count; i++)
	{
		if (contains(fb->lines[i], key))
			return i;
	}

	return -1;
}

int find_after_closing_tag(const FileBuffer* fb, int start, const char* closing_tag)
{
	int i;

	for (i = start; i < fb->count; i++)
	{
		if (contains(fb->lines[i], closing_tag))
			return i + 1;
	}

	return -1;
}

int find_socket_end(const FileBuffer* fb, int start)
{
	int i;

	for (i = start; i < fb->count; i++)
	{
		if (contains(fb->lines[i], "</Socket>"))
			return i;
	}

	return -1;
}

int already_has_socket_block_after(const FileBuffer* fb, int start, const char* socket_name)
{
	int i;
	int limit;

	limit = start + 8;
	if (fb->count < limit)
		limit = fb->count;

	for (i = start; i < limit; i++)
	{
		if (contains(fb->lines[i], socket_name))
			return 1;
	}

	return 0;
}

int replace_fuel_capacity_value(char* line, const char* new_value)
{
	char* pos;
	char* value_start;
	char* value_end;
	char new_line[MAX_LINE];
	int prefix_len;

	pos = strstr(line, "FuelCapacity=\"");
	if (NULL == pos)
		return 0;

	value_start = pos + (int)strlen("FuelCapacity=\"");
	value_end = strchr(value_start, '\"');
	if (NULL == value_end)
		return 0;

	prefix_len = (int)(value_start - line);
	if ((prefix_len + (int)strlen(new_value) + (int)strlen(value_end) + 1) >= MAX_LINE)
		return 0;

	memcpy(new_line, line, (size_t)prefix_len);
	new_line[prefix_len] = '\0';
	strcat(new_line, new_value);
	strcat(new_line, value_end);
	strcpy(line, new_line);
	return 1;
}

int patch_tuz420(void)
{
	const char* path;
	int anchor;
	int pos;
	int changed;

	path = BASE_PATH "classes/trucks/tuz_420_tatarin.xml";
	printf("Processing: %s\n", path);

	if (0 == read_file(path, &g_fb))
		return 0;

	anchor = find_line_contains(&g_fb, "Tuz420TatarinHorn", 0);
	if (0 > anchor)
	{
		printf("[WARN] anchor not found: TUZ420 horn socket\n");
		return 0;
	}

	pos = find_after_closing_tag(&g_fb, anchor, "</AddonSockets>");
	if (0 > pos)
	{
		printf("[WARN] closing </AddonSockets> not found: TUZ420\n");
		return 0;
	}

	if (already_has_socket_block_after(&g_fb, pos, "ScautTrailer"))
	{
		printf("[SKIP] TUZ420 already patched\n");
		return 1;
	}

	changed = 1;
	changed &= insert_line(&g_fb, pos, "\t<AddonSockets>\n");
	changed &= insert_line(&g_fb, pos + 1, "\t\t<Socket Names=\"ScautTrailer\" Offset=\"(-3.56; 1.43; 0)\" />\n");
	changed &= insert_line(&g_fb, pos + 2, "\t</AddonSockets>\n");

	if (0 == changed)
	{
		printf("[ERROR] insert failed: TUZ420\n");
		return 0;
	}

	if (0 == write_file(path, &g_fb))
		return 0;

	printf("[OK] TUZ420 patched\n");
	return 1;
}

int patch_ws6900(void)
{
	const char* path;
	int anchor;
	int pos;
	int changed;

	path = BASE_PATH "classes/trucks/ws_6900xd_twin.xml";
	printf("Processing: %s\n", path);

	if (0 == read_file(path, &g_fb))
		return 0;

	anchor = find_line_contains(&g_fb, "ws_6900xd_twinDiffLock", 0);
	if (0 > anchor)
	{
		printf("[WARN] anchor not found: WS6900 diff lock socket\n");
		return 0;
	}

	pos = find_after_closing_tag(&g_fb, anchor, "</AddonSockets>");
	if (0 > pos)
	{
		printf("[WARN] closing </AddonSockets> not found: WS6900\n");
		return 0;
	}

	if (already_has_socket_block_after(&g_fb, pos, "Names=\"Trailer"))
	{
		printf("[SKIP] WS6900 already patched\n");
		return 1;
	}

	changed = 1;
	changed &= insert_line(&g_fb, pos, "\t<AddonSockets>\n");
	changed &= insert_line(&g_fb, pos + 1, "\t\t<Socket Names=\"Trailer\" Offset=\"(-7.786; 0.732; 0)\" />\n");
	changed &= insert_line(&g_fb, pos + 2, "\t</AddonSockets>\n");

	if (0 == changed)
	{
		printf("[ERROR] insert failed: WS6900\n");
		return 0;
	}

	if (0 == write_file(path, &g_fb))
		return 0;

	printf("[OK] WS6900 patched\n");
	return 1;
}

int patch_azov_sprinter_socket(void)
{
	const char* path;
	int anchor;
	int pos;
	int changed;

	path = BASE_PATH "_dlc/dlc_7/classes/trucks/azov_43_191_sprinter.xml";
	printf("Processing: %s\n", path);

	if (0 == read_file(path, &g_fb))
		return 0;

	anchor = find_line_contains(&g_fb, "azovSupplies", 0);
	if (0 > anchor)
	{
		printf("[WARN] anchor not found: Azov Sprinter azovSupplies\n");
		return 0;
	}

	pos = find_after_closing_tag(&g_fb, anchor, "</AddonSockets>");
	if (0 > pos)
	{
		printf("[WARN] closing </AddonSockets> not found: Azov Sprinter\n");
		return 0;
	}

	if (already_has_socket_block_after(&g_fb, pos, "ScautTrailer"))
	{
		printf("[SKIP] Azov Sprinter socket already patched\n");
		return 1;
	}

	changed = 1;
	changed &= insert_line(&g_fb, pos, "\t<AddonSockets>\n");
	changed &= insert_line(&g_fb, pos + 1, "\t\t<Socket Names=\"ScautTrailer\" Offset=\"(-3; 0.8; 0)\" />\n");
	changed &= insert_line(&g_fb, pos + 2, "\t</AddonSockets>\n");

	if (0 == changed)
	{
		printf("[ERROR] insert failed: Azov Sprinter socket\n");
		return 0;
	}

	if (0 == write_file(path, &g_fb))
		return 0;

	printf("[OK] Azov Sprinter socket patched\n");
	return 1;
}

int patch_azov_sprinter_fuel(void)
{
	const char* path;
	int i;

	path = BASE_PATH "_dlc/dlc_7/classes/trucks/azov_43_191_sprinter_tuning/azov_43_191_sprinter_supplies.xml";
	printf("Processing: %s\n", path);

	if (0 == read_file(path, &g_fb))
		return 0;

	for (i = 0; i < g_fb.count; i++)
	{
		if (contains(g_fb.lines[i], "<TruckData") && contains(g_fb.lines[i], "FuelCapacity="))
		{
			if (0 == replace_fuel_capacity_value(g_fb.lines[i], "600"))
			{
				printf("[ERROR] failed to replace FuelCapacity in Azov Sprinter supplies\n");
				return 0;
			}

			if (0 == write_file(path, &g_fb))
				return 0;

			printf("[OK] Azov Sprinter fuel patched\n");
			return 1;
		}
	}

	printf("[WARN] <TruckData FuelCapacity=\"...\"> not found: Azov Sprinter supplies\n");
	return 0;
}

int patch_azov_5319(void)
{
	const char* path;
	int i;
	int idx_trailer_socket;
	int idx_saddle_high;
	int idx_saddle_low;
	int idx_minicrane_socket;
	int idx_socket_end;
	int frameaddon_shift_exists;
	int wrote;

	path = BASE_PATH "classes/trucks/azov_5319.xml";
	printf("Processing: %s\n", path);

	if (0 == read_file(path, &g_fb))
		return 0;

	idx_trailer_socket = -1;
	idx_saddle_high = -1;
	idx_saddle_low = -1;
	idx_minicrane_socket = -1;

	for (i = 0; i < g_fb.count; i++)
	{
		if ((0 > idx_trailer_socket) && contains(g_fb.lines[i], "<Socket Names=\"Trailer, LogTrailer"))
			idx_trailer_socket = i;

		if ((0 > idx_saddle_high) && contains(g_fb.lines[i], "NamesBlock=\"SaddleHigh") && contains(g_fb.lines[i], "Semitrailer"))
			idx_saddle_high = i;

		if ((0 > idx_saddle_low) && contains(g_fb.lines[i], "<Socket Names=\"SaddleLow"))
			idx_saddle_low = i;

		if ((0 > idx_minicrane_socket) && contains(g_fb.lines[i], "<Socket Names=\"MinicraneRU"))
			idx_minicrane_socket = i;
	}

	if (0 > idx_trailer_socket)
	{
		printf("[WARN] Trailer, LogTrailer socket not found: Azov 5319\n");
		return 0;
	}
	if (0 > idx_saddle_high)
	{
		printf("[WARN] SaddleHigh socket not found: Azov 5319\n");
		return 0;
	}
	if (0 > idx_saddle_low)
	{
		printf("[WARN] SaddleLow socket not found: Azov 5319\n");
		return 0;
	}
	if (0 > idx_minicrane_socket)
	{
		printf("[WARN] MinicraneRU socket not found: Azov 5319\n");
		return 0;
	}

	/*
	 * 当前版本：
	 * 不再改 “<AddonsShift ... Types="MinicraneRU, FrameAddon" />”
	 * 也不再找/改 idx_minicrane_shift
	 */

	strcpy(g_fb.lines[idx_saddle_high],
		"\t\t\t<Socket Names=\"Semitrailer, SemitrailerOiltank, SemitrailerFoldableLog\" NamesBlock=\"SaddleHigh\" Offset=\"(-2.685; 1.655; 0)\" >\n");

	strcpy(g_fb.lines[idx_saddle_low],
		"\t\t\t<Socket Names=\"SaddleLow\" Offset=\"(-2.671; 1.329; 0)\">\n");

	strcpy(g_fb.lines[idx_trailer_socket],
		"\t\t\t<Socket Names=\"Trailer, LogTrailer\" Offset=\"(-4.618; 1.156; 0)\" />\n");

	if ((idx_trailer_socket + 1) < g_fb.count)
	{
		if (contains(g_fb.lines[idx_trailer_socket + 1], "TrailerNamesBlock"))
			delete_line(&g_fb, idx_trailer_socket + 1);
	}

	if ((idx_trailer_socket + 1) < g_fb.count)
	{
		if (line_equals_trimmed(g_fb.lines[idx_trailer_socket + 1], "</Socket>"))
			delete_line(&g_fb, idx_trailer_socket + 1);
	}

	idx_minicrane_socket = find_line_contains(&g_fb, "<Socket Names=\"MinicraneRU", 0);
	if (0 > idx_minicrane_socket)
	{
		printf("[WARN] MinicraneRU socket disappeared after edits: Azov 5319\n");
		return 0;
	}

	idx_socket_end = find_socket_end(&g_fb, idx_minicrane_socket);
	if (0 > idx_socket_end)
	{
		printf("[WARN] closing </Socket> not found for MinicraneRU: Azov 5319\n");
		return 0;
	}

	frameaddon_shift_exists = 0;
	for (i = idx_minicrane_socket + 1; i < idx_socket_end; i++)
	{
		if (contains(g_fb.lines[i], "Types=\"FrameAddon"))
		{
			frameaddon_shift_exists = 1;
			strcpy(g_fb.lines[i],
				"\t\t\t\t<AddonsShift Offset=\"(0.25; 0; 0)\" Types=\"FrameAddon\" />\n");
			break;
		}
	}

	if (0 == frameaddon_shift_exists)
	{
		if (0 == insert_line(&g_fb, idx_socket_end,
			"\t\t\t\t<AddonsShift Offset=\"(0.25; 0; 0)\" Types=\"FrameAddon\" />\n"))
		{
			printf("[ERROR] failed to insert FrameAddon shift: Azov 5319\n");
			return 0;
		}
	}

	wrote = write_file(path, &g_fb);
	if (0 == wrote)
		return 0;

	printf("[OK] Azov 5319 patched\n");
	return 1;
}

int main(void)
{
	int ok_count;
	int total;

	ok_count = 0;
	total = 5;

	printf("=== Mod Patch Tool ===\n");
	printf("Base path: %s\n", BASE_PATH);

	if (patch_tuz420())
		ok_count++;
	if (patch_ws6900())
		ok_count++;
	if (patch_azov_sprinter_socket())
		ok_count++;
	if (patch_azov_sprinter_fuel())
		ok_count++;
	if (patch_azov_5319())
		ok_count++;

	printf("=== Done: %d/%d steps succeeded ===\n", ok_count, total);
	return 0;
}
