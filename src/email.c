/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "getopt.h"

#include "email.h"
#include "conf.h"
#include "utils.h"
#include "addr_book.h"
#include "file_io.h"
#include "message.h"
#include "error.h"
#include "mimeutils.h"

extern const char *CONTENT_DISPOSITION_INLINE;
extern const char *CONTENT_DISPOSITION_ATTACHMENT;
extern const char *CONTENT_DISPOSITION_HIDDEN;

extern const char *CONTENT_TRANSFER_ENCODING_QP;
extern const char *CONTENT_TRANSFER_ENCODING_BASE64;
extern const char *CONTENT_TRANSFER_ENCODING_8BIT;
extern const char *CONTENT_TRANSFER_ENCODING_7BIT;
extern const char *CONTENT_TRANSFER_ENCODING_BINARY;

void
defaultDestr(void *ptr)
{
	xfree(ptr);
}

static struct option Gopts[] = {
	{"attach", 1, 0, 'a'},
	{"attachment", 1, 0, 'a'},
	{"verbose", 0, 0, 'v'},
	{"high-priority", 0, 0, 'o'},
	{"encrypt", 0, 0, 'e'},
	{"subject", 1, 0, 's'},
	{"sub", 1, 0, 's'},
	{"smtp-server", 1, 0, 'r'},
	{"smtp-port", 1, 0, 'p'},
	{"conf-file", 1, 0, 'c'},
	{"check-config", 0, 0, 't'},
	{"version", 0, 0, 'v'},
	{"blank-mail", 0, 0, 'b'},
	{"smtp-user", 1, 0, 'u'},
	{"smtp-username", 1, 0, 'u'},
	{"smtp-pass", 1, 0, 'i'},
	{"smtp-password", 1, 0, 'i'},
	{"smtp-auth", 1, 0, 'm'},
	{"gpg-pass", 1, 0, 'g'},
	{"gpg-password", 1, 0, 'g'},
	{"from-addr", 1, 0, 'f'},
	{"from-name", 1, 0, 'n'},
	{"header", 1, 0, 'H'},
	{"timeout", 1, 0, 'x'},
	{"html", 0, 0, 1},
	{"sign", 0, 0, 2},
	{"clearsign", 0, 0, 2},
	{"cc", 1, 0, 3},
	{"bcc", 1, 0, 4},
	{"to-name", 1, 0, 5},
	{"tls", 0, 0, 6},
	{"no-encoding", 0, 0, 7},

	{"alt-html", 1, 0, 8},
	{"disp-type", 1, 0, 9},
	{"disp-params", 1, 0, 10},
	{"attach-related", 1, 0, 11},
	{"related-attach", 1, 0, 11},
	{"embed", 1, 0, 11},
	{"transfer-encoding", 1, 0, 'T'},
	{"subject-encoding", 1, 0, 'S'},
	{"subject-charset", 1, 0, 'S'},
	{"body-encoding", 1, 0, 'B'},
	{"body-charset", 1, 0, 'B'},
	{"mail-from-encoding", 1, 0, 'F'},
	{"mail-from-charset", 1, 0, 'F'},
	{"mail-to-encoding", 1, 0, 'R'},
	{"mail-to-charset", 1, 0, 'R'},
	{"convert-encoding-to", 1, 0, 'E'},
	{"convert-charset-to", 1, 0, 'E'},

	{NULL, 0, NULL, 0 }
};

extern int nStringBufferResizeCount, nReallocBlockChangedCount;

/**
 * Usage prints helpful usage information,
**/
void
usage(void)
{
	fprintf(stderr,
		"Options information is as follows\n"
		"email [options] recipient1,recipient2,...\n\n"
		"    -h, -help module          Print this message or specify one of the "
		"below options\n"
		"    -V, -verbose              Display mailing progress.\n"
		"    -f, -from-addr            Senders mail address\n"
		"    -n, -from-name            Senders name\n"
		"    -b, -blank-mail           Allows you to send a blank email\n"
		"    -e, -encrypt              Encrypt the e-mail for first recipient before "
		"sending\n"
		"    -s, -subject subject      Subject of message\n"
		"    -r, -smtp-server server   Specify a temporary SMTP server for sending\n"
		"    -p, -smtp-port port       Specify the SMTP port to connect to\n"
		"    -a, -attach file          Attach file and base64 encode (Can be used multiple times).\n"
		"        -disp-type disposition\n"
		"             Set disposition type for the attachment (Can be used multiple times).\n"
		"             Available values: 'attachment', 'inline', 'auto'\n"
		"        -disp-params parameters\n"
		"             Set disposition parameters that will be appended after disposition type,\n"
		"             multiple parameters can be separated by comma. Available parameters:\n"
		"             * 'f', 'n', 'fn', 'name', 'filename' for file name\n"
		"             * 'c', 'ct', 'ctime', 'creation-date' or 'creation-time' for file creation time\n"
		"             * 'm', 'mt', 'mtime', 'modification-date' or 'modification-time' for file modification time\n"
		"             * 'a', 'at', 'atime', 'r', 'rt', 'read-date', 'read-time', 'access-date', 'access-time' for file read time (last access time)\n"
		"             * 'd', 't', 'dt', 'date', 'time', 'datetime' for all the previous 3 dates/times\n"
		"             * 's', 'size' for file size\n"
		"        -attach-related file  Attach a file as a child of multipart/related content type. It's useful to embed images into HTML email. If \n"
		"    -c, -conf-file file       Path to non-default configuration file\n"
		"    -t, -check-config         Simply parse the email.conf file for errors\n"
		"    -x, -timeout              Set socket timeout.\n"
		"        -cc email,email,...   Copy recipients\n"
		"        -bcc email,email,...  Blind Copy recipients\n"
		"        -sign                 Sign the email with GPG\n"
		"        -html                 Send message in HTML format "
		"( Make your own HTML! )\n"
		"        -alt-html alternative_html_file\n"
		"                              When sending plain text mail, you can also use an HTML file as alternative content. So mail user agent can display either plain text version or HTML version content.\n"
		"        -tls                  Use TLS/SSL\n"
		"    -m, -smtp-auth type       Set the SMTP AUTH type (plain or login)\n"
		"    -u, -smtp-user username   Specify your username for SMTP AUTH\n"
		"    -i, -smtp-pass password   Specify your password for SMTP AUTH\n"
		"    -g, -gpg-pass             Specify your password for GPG\n"
		"    -H, -header string        Add header (can be used multiple times)\n"
		"        -high-priority        Send the email with high priority\n"
		"\n"
		"Content transfer encoding options:\n"
		"\n"
		"    -T, -transfer-encoding encoding\n"
		"             Specify transfer encoding. Avaiable values:\n"
		"             * 'b' or 'b64' or 'base64' for BASE64 encoding\n"
		"             * 'q' or 'qp' or 'quoted-printable' for quoted-printable encoding\n"
		"             * 'auto' for auto select BASE64 or QP based on the percentage of ASCII/7bit characters.\n"
		"               if percentage >= 75%%, then quoted-printable is used; otherwise BASE64\n"
		"             * '7', '7bit', '8', '8bit', 'bin' or 'binary' or 'raw' or 'none' or else, same as --no-encoding;\n"
		"               This will make the mail smaller than 'base64' or 'quoted-printable' encoding.\n"
		"        -no-encoding          Don't use UTF-8 encoding\n"

		"\n"
		"Character encoding options: (When your inputs use different charset...)\n"
		"\n"
		"    -S, -subject-encoding encoding\n"
		"             Specify character encoding for mail subject\n"
		"    -B, -body-encoding encoding\n"
		"             Specify character encoding for mail body\n"
		"    -F, -mail-from-encoding encoding\n"
		"             Set mail-from address encoding. mail-from address can be passed via command line or read from config file\n"
		"    -R, -mail-to-encoding encoding\n"
		"             Set mail-to or cc or bcc address encoding. this can only be passed via command line\n"
		"    -E, -convert-encoding-to encoding\n"
		"             Convert all inputs (subject, body, mail addresses) to this encoding. (If you're not sure, UTF-8 is recommeded.)\n"

		"\n"
		"\n"
		"Sample usage:\n"
		"  - Send a blank mail\n"
		"    email -smtp-server smtp.domain.com -b user@domain.com\n"
		"\n"
		"  - Send a plain text mail\n"
		"    echo \"This is mail body\" | email -smtp-server smtp.domain.com user@domain.com\n"
		"\n"
		"  - Send a plain text email with alternative HTML mail and images are embedded\n"
		"    export LC_CTYPE=zh_CN.GBK\n"
		"    echo \"Warning: Disk 1 is full. Error: Disk 2 S.M.A.R.T failed\" > plain-text.txt\n"
		"    echo \"<img src='hdd-icon.png' alt='hard-disk-drive-icon'/><span style='color:orange'>Warning: Disk 1 is full.</span> <span style='color:red'>Error: Disk 2 S.M.A.R.T failed</span>\" > html-text.txt\n"
		"    cat plain-text.txt | email -smtp-server smtp.domain.com -smtp-auth login -smtp-user my-smtp-account -smtp-pass my-smtp-password -subject \"Hardware Health Report \" -subject-encoding GBK -alt-html html-text.txt -attach-related hdd-icon.png -mail-to-encoding GBK \"TechSupport\xbc\xbc\xca\xf5\xb2\xbf<user@domain.com>\"\n"
		"\n"
	);

	exit(EXIT_SUCCESS);
}

static const char * translateContentDispositionType (const char *type)
{
	const char *translated = NULL;
	if (   strcasecmp(type, "a")==0
		|| strcasecmp(type, "attach")==0
		|| strcasecmp(type, "attachment")==0
		|| false)
		translated = CONTENT_DISPOSITION_ATTACHMENT;

	if (   strcasecmp(type, "i")==0
		|| strcasecmp(type, "inline")==0
		|| false)
		translated = CONTENT_DISPOSITION_INLINE;

	if (   strcasecmp(type, "h")==0
		|| strcasecmp(type, "hide")==0
		|| strcasecmp(type, "hidden")==0
		|| false)
		translated = CONTENT_DISPOSITION_HIDDEN;
	return translated;
}

static char * translateContentDispositionParameters (const char *params)
{
	//dlist translated;
	static char params_array[10];
	memset (params_array, 0, sizeof(params_array));
	if (   strcasecmp(params, "f")==0
		|| strcasecmp(params, "n")==0
		|| strcasecmp(params, "fn")==0
		|| strcasecmp(params, "name")==0
		|| strcasecmp(params, "filename")==0
		|| false)
		strcat (params_array, "f");

	if (   strcasecmp(params, "c")==0
		|| strcasecmp(params, "ct")==0
		|| strcasecmp(params, "ctime")==0
		|| strcasecmp(params, "creation-date")==0
		|| strcasecmp(params, "creation-time")==0
		|| false)
		strcat (params_array, "c");

	if (   strcasecmp(params, "m")==0
		|| strcasecmp(params, "mt")==0
		|| strcasecmp(params, "mtime")==0
		|| strcasecmp(params, "modification-date")==0
		|| strcasecmp(params, "modification-time")==0
		|| false)
		strcat (params_array, "m");

	if (   strcasecmp(params, "a")==0
		|| strcasecmp(params, "at")==0
		|| strcasecmp(params, "atime")==0
		|| strcasecmp(params, "r")==0
		|| strcasecmp(params, "rt")==0
		|| strcasecmp(params, "read-date")==0
		|| strcasecmp(params, "read-time")==0
		|| strcasecmp(params, "access-date")==0
		|| strcasecmp(params, "access-time")==0
		|| strcasecmp(params, "last-access-date")==0
		|| strcasecmp(params, "last-access-time")==0
		|| false)
		strcat (params_array, "a");

	if (   strcasecmp(params, "d")==0
		|| strcasecmp(params, "t")==0
		|| strcasecmp(params, "dt")==0
		|| strcasecmp(params, "date")==0
		|| strcasecmp(params, "time")==0
		|| strcasecmp(params, "datetime")==0
		|| false)
		strcat (params_array, "cma");

	if (   strcasecmp(params, "s")==0
		|| strcasecmp(params, "size")==0
		|| false)
		strcat (params_array, "s");

#ifdef _DEBUG
	printf ("params_array: %s\n", params_array);
#endif
	return params_array;
}

static const char * translateContentTransferEncoding (const char *encoding)
{
	const char *translated = NULL;
	if (   strcasecmp(encoding, "b")==0
		|| strcasecmp(encoding, "6")==0
		|| strcasecmp(encoding, "b64")==0
		|| strcasecmp(encoding, "base64")==0
		|| false)
		translated = CONTENT_TRANSFER_ENCODING_BASE64;
	else if (strcasecmp(encoding, "q")==0
		|| strcasecmp(encoding, "qp")==0
		|| strcasecmp(encoding, "quoted-printable")==0
		|| false)
		translated = CONTENT_TRANSFER_ENCODING_QP;
	else if (strcasecmp(encoding, "7")==0
		|| strcasecmp(encoding, "7bit")==0
		|| false)
		translated = CONTENT_TRANSFER_ENCODING_7BIT;
	else if (strcasecmp(encoding, "8")==0
		|| strcasecmp(encoding, "8bit")==0
		|| false)
		translated = CONTENT_TRANSFER_ENCODING_8BIT;
	else if (strcasecmp(encoding, "bin")==0
		|| strcasecmp(encoding, "binary")==0
		|| strcasecmp(encoding, "raw")==0
		|| false)
		translated = CONTENT_TRANSFER_ENCODING_BINARY;

	return translated;
}

char *
getConfValue(const char *tok)
{
	return (char *)dhGetItem(table, tok);
}

void
setConfValue(const char *tok, const char *val)
{
	dhInsert(table, tok, val);
}


/**
 * ModuleUsage will take an argument for the specified 
 * module and print out help information on the topic.  
 * information is stored in a written file in the location 
 * in Macro EMAIL_DIR. and also specified with EMAIL_HELP_FILE
 */
static void
moduleUsage(const char *module)
{
	FILE *help=NULL;
	short found=0;
	char *moduleptr=NULL;
	dstrbuf *buf = DSB_NEW;
	dstrbuf *help_file = expandPath(EMAIL_HELP_FILE);

	if (!(help = fopen(help_file->str, "r"))) {
		fatal("Could not open help file: %s", help_file->str);
		dsbDestroy(help_file);
		properExit(ERROR);
	}
	dsbDestroy(help_file);

	while (!feof(help)) {
		dsbReadline(buf, help);
		if ((buf->str[0] == '#') || (buf->str[0] == '\n')) {
			continue;
		}

		chomp(buf->str);
		moduleptr = strtok(buf->str, "|");
		if (strcasecmp(moduleptr, module) != 0) {
			while ((moduleptr = strtok(NULL, "|")) != NULL) {
				if (strcasecmp(moduleptr, module) == 0) {
					found = 1;
					break;
				}
			}
		} else {
			found = 1;
		}

		if (!found) {
			continue;
		}
		while (!feof(help)) {
			dsbReadline(buf, help);
			if (!strcmp(buf->str, "EOH\n")) {
				break;
			}
			printf("%s", buf->str);
		}
		break;
	}

	if (feof(help)) {
		printf("There is no help in the module: %s\n", module);
		usage();
	}

	dsbDestroy(buf);
	fclose(help);
	exit(0);
}


int
main(int argc, char **argv)
{
	int get;
	int opt_index = 0;          /* for getopt */
	char *cc_string = NULL;
	char *bcc_string = NULL;
	const char *opts = "f:n:a:p:oVedvtb?c:s:r:u:i:g:m:H:x:T:S:B:F:R:E:";

	/* Set certian global options to NULL */
	conf_file = NULL;
	global_msg = NULL;
	memset(&Mopts, 0, sizeof(struct mailer_options));
	Mopts.encoding = true;

	/* Check if they need help */
	if ((argc > 1) && (!strcmp(argv[1], "-h") ||
		!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"))) {
		if (argc == 3) {
			moduleUsage(argv[2]);
		} else if (argc == 2) {
			usage();
		} else {
			fprintf(stderr, "Only specify one option with %s: \n", argv[1]);
			usage();
		}
	}

	table = dhInit(28, defaultDestr);
	if (!table) {
		fprintf(stderr, "ERROR: Could not initialize Hash table.\n");
		exit(0);
	}

	while ((get = getopt_long_only(argc, argv, opts, Gopts, &opt_index)) > EOF) {
		switch (get) {
		case 'n':
			setConfValue("MY_NAME", xstrdup(optarg));
			break;
		case 'f':
			setConfValue("MY_EMAIL", xstrdup(optarg));
			break;
		case 'a':
			if (!Mopts.attach) {
				Mopts.attach = dlInit(defaultDestr);
			}
			dlInsertTop(Mopts.attach, xstrdup(optarg));
			break;
		case 'V':
			Mopts.verbose = true;
			break;
		case 'p':
			setConfValue("SMTP_PORT", xstrdup(optarg));
			break;
		case 'o':
			Mopts.priority = 1;
			break;
		case 'e':
			Mopts.gpg_opts |= GPG_ENC;
			break;
		case 's':
			Mopts.subject = optarg;
			break;
		case 'r':
			setConfValue("SMTP_SERVER", xstrdup(optarg));
			break;
		case 'c':
			conf_file = optarg;
			break;
		case 't':
			checkConfig();
			printf("Configuration file is proper.\n");
			dhDestroy(table);
			return (0);
			break;
		case 'v':
			printf("email - By Dean Jones; Version %s\n", EMAIL_VERSION);
			dhDestroy(table);
			exit(EXIT_SUCCESS);
			break;
		case 'b':
			Mopts.blank = 1;
			break;
		case 'u':
			setConfValue("SMTP_AUTH_USER", xstrdup(optarg));
			break;
		case 'i':
			setConfValue("SMTP_AUTH_PASS", xstrdup(optarg));
			break;
		case 'm':
			setConfValue("SMTP_AUTH", xstrdup(optarg));
			break;
		case 'g':
			setConfValue("GPG_PASS", xstrdup(optarg));
			break;
		case 'H':
			if (!Mopts.headers) {
				Mopts.headers = dlInit(defaultDestr);
			}
			dlInsertTop(Mopts.headers, xstrdup(optarg));
			break;
		case 'x':
			setConfValue("TIMEOUT", xstrdup(optarg));
			break;
		case '?':
			usage();
			break;
		case 1:
			Mopts.html = 1;
			break;
		case 2:
			Mopts.gpg_opts |= GPG_SIG;
			break;
		case 3:
			cc_string = optarg;
			break;
		case 4:
			bcc_string = optarg;
			break;
		case 5:
			/* To name? */
			break;
		case 6:
			setConfValue("USE_TLS", xstrdup("true"));
			break;
		case 7:
			Mopts.encoding = false;
			break;

		case 8:
#ifdef _DEBUG
			printf ("Alternative HTML file is [%s]\n", optarg);
#endif
			Mopts.alt_html_filename = xstrdup(optarg);
			break;
		case 9:
#ifdef _DEBUG
			printf ("Attachment disposition is [%s]\n", optarg);
#endif
			translateContentDispositionType (optarg);
			break;
		case 10:
#ifdef _DEBUG
			printf ("Attachment disposition parameters are [%s]\n", optarg);
#endif
			translateContentDispositionParameters (optarg);
			break;
		case 11:
#ifdef _DEBUG
			printf ("Attach related file is [%s]\n", optarg);
#endif
			if (!Mopts.related_attach) {
				Mopts.related_attach = dlInit(defaultDestr);
			}
			dlInsertTop(Mopts.related_attach, xstrdup(optarg));
			break;
		case 'T':
#ifdef _DEBUG
			printf ("Content transfer encoding is [%s]\n", optarg);
#endif
			//setConfValue("TRANSFER_ENCODING", translateContentTransferEncoding(optarg));
			Mopts.transfer_encoding = translateContentTransferEncoding(optarg);
			break;
		case 'S':
#ifdef _DEBUG
			printf ("Mail subject encoding is [%s]\n", optarg);
#endif
			setConfValue("SUBJECT_ENCODING", xstrdup(optarg));
			Mopts.subject_encoding = optarg;
			break;
		case 'B':
#ifdef _DEBUG
			printf ("Mail body encoding is %s\n", optarg);
#endif
			setConfValue("BODY_ENCODING", xstrdup(optarg));
			Mopts.body_encoding = optarg;
			break;
		case 'F':
#ifdef _DEBUG
			printf ("Mail-from encoding is %s\n", optarg);
#endif
			setConfValue("MAIL_FROM_ENCODING", xstrdup(optarg));
			Mopts.mail_from_encoding = optarg;
			break;
		case 'R':
#ifdef _DEBUG
			printf ("Recipients encoding is %s\n", optarg);
#endif
			setConfValue("MAIL_TO_ENCODING", xstrdup(optarg));
			Mopts.mail_to_encoding = optarg;
			break;
		case 'E':
#ifdef _DEBUG
			printf ("Convert encoding to %s\n", optarg);
#endif
			setConfValue("CONVERT_ENCODING_TO", xstrdup(optarg));
			Mopts.convert_encoding_to = optarg;
			break;
		default:
			/* Print an error message here  */
			usage();
			break;
		}
	}

	/* first let's check to make sure they specified some recipients */
	if (optind == argc) {
		usage();
	}

	configure();

	/* Check to see if we need to attach a vcard. */
	if (getConfValue("VCARD")) {
		dstrbuf *vcard = expandPath(getConfValue("VCARD"));
		if (!Mopts.attach) {
			Mopts.attach = dlInit(defaultDestr);
		}
		dlInsertTop(Mopts.attach, xstrdup(vcard->str));
		dsbDestroy(vcard);
	}

	/* set to addresses if argc is > 1 */
	if (!(Mopts.to = getNames(argv[optind]))) {
		fatal("You must specify at least one recipient!\n");
		properExit(ERROR);
	}

	/* Set CC and BCC addresses */
	if (cc_string) {
		Mopts.cc = getNames(cc_string);
	}
	if (bcc_string) {
		Mopts.bcc = getNames(bcc_string);
	}

	signal(SIGTERM, properExit);
	signal(SIGINT, properExit);
	signal(SIGPIPE, properExit);
	signal(SIGHUP, properExit);
	signal(SIGQUIT, properExit);

	createMail();
#ifdef _DEBUG
	printf ("nStringBufferResizeCount=%d, nReallocBlockChangedCount=%d\n", nStringBufferResizeCount, nReallocBlockChangedCount);
#endif
	properExit(0);

	/* We never get here, but gcc will whine if i don't return something */
	return 0;
}

