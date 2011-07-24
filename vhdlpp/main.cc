
const char COPYRIGHT[] =
          "Copyright (c) 2011 Stephen Williams (steve@icarus.com)";
/*
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
# include  "vhdlpp_config.h"
# include  "version_base.h"
# include  "version_tag.h"

/*
 * Usage:  vhdlpp [flags] sourcefile...
 * Flags:
 *
 **  -D <token>
 *     This activates various sorts of debugging aids. The <token>
 *     specifies which debugging aid to activate. Valid tokens are:
 *
 *     yydebug | no-yydebug
 *        Enable (disable) debug prints from the bison parser
 *
 *     libraries=<path>
 *        Enable debugging of library support by dumping library
 *        information to the file named <path>.
 *
 *     entities=<path>
 *        Enable debugging of elaborated entities by writing the
 *        elaboration results to the file named <path>.
 *
 **  -v
 *     Verbose operation. Display verbose non-debug information.
 *
 **  -V
 *     Version. Print the version of this binary.
 *
 **  -w <path>
 *     Work path. This sets the path to the working library
 *     directory. I write into that directory files for packages that
 *     I declare, and I read from that directory packages that are
 *     already declared. The default path is "ivl_vhdl_work".
 */

const char NOTICE[] =
"  This program is free software; you can redistribute it and/or modify\n"
"  it under the terms of the GNU General Public License as published by\n"
"  the Free Software Foundation; either version 2 of the License, or\n"
"  (at your option) any later version.\n"
"\n"
"  This program is distributed in the hope that it will be useful,\n"
"  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"  GNU General Public License for more details.\n"
"\n"
"  You should have received a copy of the GNU General Public License along\n"
"  with this program; if not, write to the Free Software Foundation, Inc.,\n"
"  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n"
;

# include  "compiler.h"
# include  "parse_api.h"
# include  "vtype.h"
# include  <fstream>
# include  <cstdio>
# include  <cstdlib>
# include  <cstring>
# include  <cerrno>
#if defined(HAVE_GETOPT_H)
# include  <getopt.h>
#endif
# include  <sys/stat.h>


bool verbose_flag = false;
  // Where to dump design entities
const char*dump_design_entities_path = 0;
const char*dump_libraries_path = 0;


extern void dump_libraries(ostream&file);
extern void parser_cleanup();

static void process_debug_token(const char*word)
{
      if (strcmp(word, "yydebug") == 0) {
	    yydebug = 1;
      } else if (strcmp(word, "no-yydebug") == 0) {
	    yydebug = 0;
      } else if (strncmp(word, "entities=", 9) == 0) {
	    dump_design_entities_path = strdup(word+9);
      } else if (strncmp(word, "libraries=", 10) == 0) {
	    dump_libraries_path = strdup(word+10);
      }
}

int main(int argc, char*argv[])
{
      int opt;
      int rc;
      const char*work_path = "ivl_vhdl_work";

      while ( (opt=getopt(argc, argv, "D:vVw:")) != EOF) switch (opt) {

	  case 'D':
	    process_debug_token(optarg);
	    break;

	  case 'v':
	    fprintf(stderr, "Icarus Verilog VHDL Parse version "
		    VERSION " (" VERSION_TAG ")\n\n");
	    fprintf(stderr, "%s\n\n", COPYRIGHT);
	    fputs(NOTICE, stderr);
	    verbose_flag = true;
	    break;

	  case 'V':
	    fprintf(stdout, "Icarus Verilog VHDL Parse version "
		    VERSION " (" VERSION_TAG ")\n\n");
	    fprintf(stdout, "%s\n\n", COPYRIGHT);
	    fputs(NOTICE, stdout);
	    break;

	  case 'w':
	    work_path = optarg;
	    break;
      }

      if ( (rc = mkdir(work_path, 0777)) < 0 ) {
	    if (errno != EEXIST) {
		  fprintf(stderr, "Icarus Verilog VHDL unable to create work directory %s, errno=%d\n", work_path, errno);
		  return -1;
	    }
	    struct stat stat_buf;
	    rc = stat(work_path, &stat_buf);

	    if (!S_ISDIR(stat_buf.st_mode)) {
		  fprintf(stderr, "Icarus Verilog VHDL work path `%s' is not a directory.\n", work_path);
		  return -1;
	    }
      }

      library_set_work_path(work_path);

      preload_global_types();
      int errors = 0;

      for (int idx = optind ; idx < argc ; idx += 1) {
	    parse_errors = 0;
	    parse_sorrys = 0;
	    rc = parse_source_file(argv[idx], false);
	    if (rc < 0)
		  return 1;

	    if (verbose_flag)
		  fprintf(stderr, "parse_source_file() returns %d, parse_errors=%d, parse_sorrys=%d\n", rc, parse_errors, parse_sorrys);

	    if (parse_errors > 0) {
		  fprintf(stderr, "Encountered %d errors parsing %s\n", parse_errors, argv[idx]);
	    }
	    if (parse_sorrys > 0) {
		  fprintf(stderr, "Encountered %d unsupported constructs parsing %s\n", parse_sorrys, argv[idx]);
	    }

	    if (parse_errors || parse_sorrys) {
		  errors += parse_errors;
		  errors += parse_sorrys;
		  break;
	    }
      }

      if (dump_libraries_path) {
	    ofstream file(dump_libraries_path);
	    dump_libraries(file);
      }

      if (dump_design_entities_path) {
	    ofstream file(dump_design_entities_path);
	    dump_design_entities(file);
      }

      if (errors > 0) {
        parser_cleanup();
	    return 2;
      }

      errors = elaborate_entities();
      if (errors > 0) {
	    fprintf(stderr, "%d errors elaborating design.\n", errors);
        parser_cleanup();
	    return 3;
      }

      errors = emit_entities();
      if (errors > 0) {
	    fprintf(stderr, "%d errors emitting design.\n", errors);
        parser_cleanup();
	    return 4;
      }

      parser_cleanup();
      return 0;
}
