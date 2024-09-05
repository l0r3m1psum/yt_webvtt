/* A reader for YouTube's auto-generated WebVTT files.
 * cc -g yt_webvtt.c -o yt_webvtt && lldb yt_webvtt --local-lldbinit -o run
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define BREAKPOINT __builtin_debugtrap()

/* String library */

/* Put this lines in your .lldbinit
 * command regex ps 's/(.+)/p *(char(*)[`(%1).len`])(%1).chars/'
 * setting set target.max-string-summary-length 10
 */
typedef struct {
	const char *chars;
	size_t len;
} Str;

/* Needed for strict compliance with C99
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105510#c4
 * and being able to compile with MSVC.
 */
#define S_(string_litteral) \
	{ \
		.chars = (string_litteral), \
		.len = sizeof(string_litteral) - 1, \
	}
#define S(string_litteral) \
	((Str) S_(string_litteral))

/* Macros for printf. */
#define STR_FMT "%.*s"
#define STR_ARG(s) ((int) (s).len), ((s).chars)

static bool xisdigit(char c) {
	return '0' <= c && c <= '9';
}

static bool Str_invariant(Str s) {
	return s.chars != NULL;
}

static bool Str_equal(Str s1, Str s2) {
	assert(Str_invariant(s1));
	assert(Str_invariant(s2));

	if (s1.len != s2.len) {
		return false;
	}

	for (size_t i = 0; i < s1.len; i++) {
		if (s1.chars[i] != s2.chars[i]) {
			return false;
		}
	}

	return true;
}

static Str Str_slice_front(Str s, size_t amount) {
	assert(Str_invariant(s));
	assert(amount <= s.len);

	Str res = {.chars = s.chars + amount, .len = s.len - amount};
	return res;
}

static Str Str_slice_back(Str s, size_t amount) {
	assert(Str_invariant(s));
	assert(amount <= s.len);

	Str res = {.chars = s.chars, .len = s.len - amount};
	return res;
}

static Str Str_slice(Str s, size_t start, size_t end) {
	assert(Str_invariant(s));
	assert(start <= end);
	assert(end <= s.len);

	Str res = {.chars = s.chars + start, .len = end - start};
	return res;
}

static bool Str_starts_with(Str s, Str what) {
	assert(Str_invariant(s));
	assert(Str_invariant(what));

	if (what.len > s.len) {
		return false;
	}

	for (size_t i = 0; i < what.len; i++) {
		if (s.chars[i] != what.chars[i]) {
			return false;
		}
	}

	return true;
}

static ssize_t Str_find_str(Str s, Str what) {
	assert(Str_invariant(s));
	assert(Str_invariant(what));

	if (what.len > s.len) {
		return -1;
	}

	for (size_t i = 0; i < s.len - what.len + 1; i++) {
		if (s.chars[i] != what.chars[0]) {
			continue;
		}
		size_t j = 1;
		for (; j < what.len; j++) {
			if (s.chars[i+j] != what.chars[j]) {
				goto not_equal;
			}
		}
		return i;
not_equal:
		i += j;
	}

	return -1;
}

static int Str_println(Str s) {
	return printf("%.*s\n", (int) s.len, s.chars);
}

/* Bump allocator library */

typedef struct {
	void *mem;
	size_t size;
	size_t used;
} Buf;

static void *Buf_bump(Buf *alloc, size_t req_size) {
	assert(alloc);
	if (alloc->size - alloc->used < req_size) {
		return NULL;
	}
	void *res = (unsigned char *)alloc->mem + alloc->used;
	alloc->used += req_size;
	return res;
}

static void Buf_free(Buf *alloc) {
	assert(alloc);
	alloc->used = 0;
}


/* WebVTT reading */

const Str webvtt_magic            = S_("WEBVTT");
const Str webvtt_end_marker       = S_("\n\n");
const Str webvtt_timing_format    = S_("00:00:00.000 --> 00:00:00.000");
const Str webvtt_timestamp_separator = S_(" --> ");
const Str webvtt_timestamp_format = S_("00:00:00.000");
const long webvtt_timestamp_radixes[] = {3600000, 60000, 1000, 1};

typedef enum {
	/* Here BAD means either malformed or absent. */
	WEBVTT_OK,
	WEBVTT_NO_MEM,
	WEBVTT_BAD_MAGIC,
	WEBVTT_BAD_BLOCK,
	WEBVTT_BAD_TIMING,
	WEBVTT_BAD_TIMESTAMP,
	WEBVTT_BAD_PAYLOAD,
} Webvtt_error;

typedef struct {
	size_t pos;
	unsigned long time_ms; // TODO: make sure that a unsigned long is enough.
} Webvtt_timestap;

static long webvtt_timestamp_to_ms(Str timestamp) {
	if (timestamp.len != webvtt_timestamp_format.len) {
		return -1;
	}

	if (!xisdigit(timestamp.chars[0]))  {return -1;}
	if (!xisdigit(timestamp.chars[1]))  {return -1;}
	if (timestamp.chars[2] != ':')      {return -1;}
	if (!xisdigit(timestamp.chars[3]))  {return -1;}
	if (!xisdigit(timestamp.chars[4]))  {return -1;}
	if (timestamp.chars[5] != ':')      {return -1;}
	if (!xisdigit(timestamp.chars[6]))  {return -1;}
	if (!xisdigit(timestamp.chars[7]))  {return -1;}
	if (timestamp.chars[8] != '.')      {return -1;}
	if (!xisdigit(timestamp.chars[9]))  {return -1;}
	if (!xisdigit(timestamp.chars[10])) {return -1;}
	if (!xisdigit(timestamp.chars[11])) {return -1;}

	enum {
		HOUR,
		MIN,
		SEC,
		MS,
		UNITS_NO
	};

	long times[UNITS_NO] = {0};
	times[HOUR] = (timestamp.chars[0] - '0')*10 + (timestamp.chars[1] - '0');
	times[MIN]  = (timestamp.chars[3] - '0')*10 + (timestamp.chars[4] - '0');
	times[SEC]  = (timestamp.chars[6] - '0')*10 + (timestamp.chars[7] - '0');
	times[MS]   = (timestamp.chars[9] - '0')*100
		+ (timestamp.chars[10] - '0')*10
		+ (timestamp.chars[11] - '0');

	if (times[MIN] > 59) {
		return -1;
	}
	if (times[SEC] > 59) {
		return -1;
	}

	long res = 0;
	for (int i = 0; i < UNITS_NO; i++) {
		res += times[i]*webvtt_timestamp_radixes[i];
	}

	return res;
}

/* Run this lldb command to set a breakpoint on every return statement of the
 * function: br set -X read_webvtt -p return
 * frame variable
 */
static Webvtt_error read_webvtt(
	Str data,
	Buf *mem,
	char **clean_text_output,
	size_t *clean_text_output_len,
	Webvtt_timestap **timestaps_output,
	size_t *timestaps_output_len) {

	/* We over-allocate space to put the cleaned text. We cannot have more clean
	 * text than the data in the file.
	 */
	size_t clean_text_len = data.len;
	size_t clean_text_index = 0;
	char *clean_text = Buf_bump(mem, sizeof *clean_text * clean_text_len);
	if (clean_text == NULL) {
		return WEBVTT_NO_MEM;
	}

	/* We over-allocate the number of timestamps needed. We cannot have more
	 * timestamps than one per line.
	 * For all i > 0 timestaps[i-1].pos < timestamps[i].pos.
	 */
	size_t timestaps_len = data.len/(webvtt_timestamp_format.len+1);
	size_t timestaps_index = 0;
	Webvtt_timestap *timestaps = Buf_bump(mem, sizeof *timestaps * timestaps_len);
	if (timestaps == NULL) {
		Buf_free(mem);
		return WEBVTT_NO_MEM;
	}

	/* We use this allocator to create and destroy temporary string buffers in
	 * the parsing loop.
	 */
	Buf text_mem = {0};
	{
		size_t remaining_memory_len = mem->size - mem->used;
		void *remaining_memory = Buf_bump(mem, remaining_memory_len);
		text_mem = (Buf) {
			.mem = remaining_memory,
			.size = remaining_memory_len,
			.used = 0,
		};
	}

	if (!Str_starts_with(data, webvtt_magic)) {
		Buf_free(mem);
		return WEBVTT_BAD_MAGIC;
	}

	Str s = Str_slice_front(data, webvtt_magic.len);

	ssize_t block_end = Str_find_str(s, webvtt_end_marker);
	if (block_end == -1) {
		Buf_free(mem);
		return WEBVTT_BAD_BLOCK;
	}

	s = Str_slice_front(s, block_end + webvtt_end_marker.len);

	/* We ignore the header block. */

	while (true) {
		/* We skip both comments and style blocks. */
		if (Str_starts_with(s, S("NOTE")) || Str_starts_with(s, S("STYLE"))) {
			block_end = Str_find_str(s, webvtt_end_marker);
			if (block_end == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_BLOCK;
			}
			goto find_next_block;
		}

		/* Reading the timing line. */
		{
			/* Since no video last more than one our we use this "trick" to find the
			 * line containing the cue timing.
			 */
			if (!Str_starts_with(s, S("0"))) {
				/* The first line could be the cue identifier. In case we ignore it.
				 */
				ssize_t timing_start = Str_find_str(s, S("\n"));
				if (timing_start == -1) {
					Buf_free(mem);
					return WEBVTT_BAD_TIMING;
				}
				s = Str_slice_front(s, timing_start+1);
				if (Str_starts_with(s, S("0"))) {
					Buf_free(mem);
					return WEBVTT_BAD_TIMING;
				}
			}

			ssize_t timing_line_end = Str_find_str(s, S("\n"));
			if (timing_line_end == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_TIMING;
			}
			if (timing_line_end < webvtt_timing_format.len) {
				Buf_free(mem);
				return WEBVTT_BAD_TIMING;
			}

			Str timing = Str_slice(s, 0, webvtt_timing_format.len);

			ssize_t testamp_separator = Str_find_str(timing, webvtt_timestamp_separator);
			if (testamp_separator == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_TIMING;
			}

			Str start_timestamp = Str_slice(timing, 0, webvtt_timestamp_format.len);
			Str end_timestamp = Str_slice(timing, webvtt_timestamp_format.len
				+ webvtt_timestamp_separator.len, webvtt_timing_format.len);

			long start_timestamp_ms = webvtt_timestamp_to_ms(start_timestamp);
			if (start_timestamp_ms == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_TIMESTAMP;
			}
			long end_timestamp_ms = webvtt_timestamp_to_ms(end_timestamp);
			if (end_timestamp_ms == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_TIMESTAMP;
			}

			s = Str_slice_front(s, timing_line_end + 1);
		}

		/* Reading the payload. */
		{
			block_end = Str_find_str(s, webvtt_end_marker);
			if (block_end == -1) {
				Buf_free(mem);
				return WEBVTT_BAD_PAYLOAD;
			}
			Str payload = Str_slice(s, 0, block_end);

			/* Again we over-allocate some space to put the cleaned payload in.
			 */
			size_t tmp_clean_text_index = 0;
			char *tmp_clean_text = Buf_bump(&text_mem, sizeof (char) * payload.len);
			if (tmp_clean_text == NULL) {
				Buf_free(mem);
				return WEBVTT_NO_MEM;
			}

			/* Here we clean the payload in the sense that we substitute every
			 * '\n' with a ' ', we remove the tags and if there is more than one
			 * consecutive ' ' we collapse them. Timestamps positions are
			 * calculated relative to tmp_clean_text and are updated later.
			 */
			size_t timestaps_no = 0;
			size_t skipped_chars = 0;
			bool in_tag = false;
			char prev_cleaned_char = ' ';
			for (size_t i = 0; i < payload.len; i++) {
				char c = payload.chars[i];
				c = (c != '\n') ? c : ' ';
				if (c == '<') {
					if (in_tag) {
						Buf_free(mem);
						return WEBVTT_BAD_PAYLOAD;
					}
					in_tag = true;
					skipped_chars++;
					continue;
				} else if (c == '>') {
					if (!in_tag) {
						Buf_free(mem);
						return WEBVTT_BAD_PAYLOAD;
					}
					in_tag = false;
					skipped_chars++;
					continue;
				}
				if (in_tag) {
					assert(i > 0);
					/* Again same trick for timestamps. */
					if (c == '0' && payload.chars[i-1] == '<') {
						size_t remaining_length = payload.len - i;
						if (remaining_length <= webvtt_timestamp_format.len) {
							Buf_free(mem);
							return WEBVTT_BAD_TIMESTAMP;
						}
						Str timestamp = Str_slice(payload, i,
							i + webvtt_timestamp_format.len);
						long timestamp_ms = webvtt_timestamp_to_ms(timestamp);
						if (timestamp_ms == -1) {
							Buf_free(mem);
							return WEBVTT_BAD_TIMESTAMP;
						}
						timestaps_no++;
						assert(i - skipped_chars <= tmp_clean_text_index);
						timestaps[timestaps_index++] = (Webvtt_timestap){
							.pos = i - skipped_chars,
							.time_ms = timestamp_ms,
						};
						// TODO: if the file terminates with a timestamp i
						// whould point 1 character out of the clean_text
						// string.  We should check for this kind of error.
					}
					skipped_chars++;
					continue;
				}
				if (c == ' ' && prev_cleaned_char == ' ') {
					skipped_chars++;
					continue;
				}
				tmp_clean_text[tmp_clean_text_index++] = c;
				prev_cleaned_char = c;
			}

			/* We try to see if there is some repetition in the start of the new
			 * string with the ending of the clean text.
			 */
			Str left = {.chars = clean_text, .len = clean_text_index};
			Str right = {.chars = tmp_clean_text, .len = tmp_clean_text_index};

			ssize_t li = 0; /* left index */
			ssize_t ri = 0; /* right index */

			if (left.len > right.len) {
				li = left.len - right.len;
				ri = right.len;
			} else {
				li = 0;
				ri = left.len;
			}

			bool overlap = false;
			while (ri > 0) {
				assert(li < left.len);
				if (Str_equal(
						Str_slice_front(left, li),
						Str_slice(right, 0, ri)
					)) {
					overlap = true;
					break;
				}
				li++;
				ri--;
			}

			// FIXME: this two strings "we're very open to that"
			// "thank you mr chair" fool the current algorithm. a possible
			// solution could be to try to match word by word but right now is
			// too complicated and I do not want to think about it. It is fine
			// to have minor errors in the data.
			size_t start = overlap ? ri : 0;
			for (size_t i = start; i < right.len; i++) {
				clean_text[clean_text_index++] = right.chars[i];
			}
			ssize_t prev_pos = -1;
			long prev_time_ms = -1;
			for (size_t i = timestaps_index - timestaps_no;
				i < timestaps_index; i++) {
				assert(left.len >= start);
				timestaps[i].pos += left.len - start;
				/* If we meet a bad timestamp in the data we try to fix it. */
				if (prev_time_ms >= (long) timestaps[i].time_ms) {
					assert(i != 0);
					timestaps[i].time_ms = prev_time_ms + 1;
				}
				assert(prev_pos < (ssize_t) timestaps[i].pos);
				assert(prev_time_ms < (long) timestaps[i].time_ms);
				assert(timestaps[i].pos < clean_text_index);
				prev_pos = timestaps[i].pos;
				prev_time_ms = timestaps[i].time_ms;
			}

			Buf_free(&text_mem);
		}

find_next_block:
		/* We have reached the end of the data (s) and it was well formed. */
		if (block_end + webvtt_end_marker.len == s.len) {
			*clean_text_output = clean_text;
			*clean_text_output_len = clean_text_index;
			*timestaps_output = timestaps;
			*timestaps_output_len = timestaps_index;
			return WEBVTT_OK;
		}
		s = Str_slice_front(s, block_end + webvtt_end_marker.len);
	}
}

#ifdef TEST
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <sys/stat.h>

	int
	main(void) {
		int fd = open("data/FOMC press conference, November 3, 2021.en.vtt", O_RDONLY);
		if (fd == -1) {
			perror("open");
			return 1;
		}
		struct stat stat_buf = {0};
		if (fstat(fd, &stat_buf) == -1) {
			perror("fstat");
			return 1;
		}
		const char *data = NULL;
		{
			off_t offset = 0;
			if ((data = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd,
					offset)) == MAP_FAILED) {
				perror("mmap");
				return 1;
			}
		}

		void *memory = NULL;
		size_t gibibyte = 1L << 30;
		{
			int fd = -1;
			off_t offset = 0;
			if ((memory = mmap(NULL, gibibyte, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, fd, offset)) == MAP_FAILED) {
				perror("mmap");
				return 1;
			}
		}

		Str file = {.chars = data, .len = stat_buf.st_size};
		Buf scratch_memory = {.mem = memory, .size = gibibyte, .used = 0};

		// const char *clean_text = NULL;
		// const Webvtt_timestap *timestaps = NULL;
		Webvtt_error res = read_webvtt(file, &scratch_memory);
		if (res != WEBVTT_OK) {
			return 1;
		}
		return 0;
	}
#endif
