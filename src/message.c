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
#include <unistd.h>
#include <fcntl.h>

#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Autoconf manual suggests this. */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "email.h"
#include "execgpg.h"
#include "utils.h"
#include "file_io.h"
#include "addr_book.h"
#include "remotesmtp.h"
#include "addr_parse.h"
#include "message.h"
#include "mimeutils.h"
#include "error.h"

const char *CONTENT_PRIMARY_TYPE_TEXT        = "text";
const char *CONTENT_PRIMARY_TYPE_MULTIPART   = "multipart";
const char *CONTENT_PRIMARY_TYPE_APPLICATION = "application";
const char *CONTENT_PRIMARY_TYPE_MESSAGE     = "message";

// text/
const char *CONTENT_SUBTYPE_PLAIN            = "plain";
const char *CONTENT_SUBTYPE_HTML             = "html";
// multipart/
const char *CONTENT_SUBTYPE_MIXED            = "mixed";
const char *CONTENT_SUBTYPE_RELATED          = "related";
const char *CONTENT_SUBTYPE_ALTERNATIVE      = "alternative";
const char *CONTENT_SUBTYPE_SIGNED           = "signed";
const char *CONTENT_SUBTYPE_ENCRYPTED        = "encrypted";
// application/
const char *CONTENT_SUBTYPE_PGP_ENCRYPTED    = "pgp-encrypted";
const char *CONTENT_SUBTYPE_PGP_SIGNATURE    = "pgp-signature";
const char *CONTENT_SUBTYPE_PGP_KEYS         = "pgp-keys";
const char *CONTENT_SUBTYPE_PKCS7_ENCRYPTED  = "pkcs7-encrypted";
const char *CONTENT_SUBTYPE_PKCS7_SIGNATURE  = "pkcs7-signature";
const char *CONTENT_SUBTYPE_OCTET_STREAM     = "octet-stream";
// message/
const char *CONTENT_SUBTYPE_RFC822           = "rfc822";
const char *CONTENT_SUBTYPE_PARTIAL          = "partial";

const char *CONTENT_DISPOSITION_INLINE       = "inline";
const char *CONTENT_DISPOSITION_ATTACHMENT   = "attachment";
const char *CONTENT_DISPOSITION_HIDDEN       = "hidden";

const char *CONTENT_TRANSFER_ENCODING_QP     = "quoted-printable";
const char *CONTENT_TRANSFER_ENCODING_BASE64 = "base64";
const char *CONTENT_TRANSFER_ENCODING_8BIT   = "8bit";
const char *CONTENT_TRANSFER_ENCODING_7BIT   = "7bit";
const char *CONTENT_TRANSFER_ENCODING_BINARY = "binary";

/**
 * this is the function that takes over from main().
 * It will call all functions nessicary to finish off the
 * rest of the program and then return properly.
**/
void createMail(void)
{
	dstrbuf *msg=NULL;
	char subject[MAXBUF]={0};

	/**
	 * first let's check if someone has tried to send stuff in from STDIN
	 * if they have, let's call a read to stdin
	 */
	if (isatty(STDIN_FILENO) == 0) {
		msg = readInput();
		if (!msg) {
			fatal("Problem reading from STDIN redirect\n");
			properExit(ERROR);
		}
	} else {
		/* If they aren't sending a blank email */
		if (!Mopts.blank) {
			/* let's check if they want to add a subject or not */
			if (Mopts.subject == NULL) {
				fprintf(stderr, "Subject: ");
				fgets(subject, sizeof(subject)-1, stdin);
				chomp(subject);
				Mopts.subject = subject;
			}

			/* Now we need to let them create a file */
			msg = editEmail();
			if (!msg) {
				properExit(ERROR);
			}
		} else {
			/* Create a blank message */
			msg = DSB_NEW;
		}
	}

	MimeMessage *mm = newMimeMessage (msg);

	dstrbuf *buf = DSB_NEW;
	printMimeMultiPart (mm, buf, true, false);

	global_msg = dsbNew (buf->len);
	provisionForDataTransparency (global_msg, buf->str);
#ifdef _DEBUG
	printf (global_msg->str);
#endif

	if (!global_msg) {
		dsbDestroy(msg);
		properExit(ERROR);
	}

	dsbDestroy(msg);
	sendmail(global_msg);
}

/*
construct MIME message structure, prepare data (encoding, reading file, etc)

+---- multipart/encrypted or signed ----+
| +--------- multipart/mixed ---------+ |
| | +------ multipart/related ------+ | |
| | | +-- multipart/alternative --+ | | |
| | | |  +------------+           | | | |
| | | |  | plain text |           | | | |
| | | |  +------------+           | | | |
| | | |  +------------+           | | | |
| | | |  |    HTML    |           | | | |
| | | |  +------------+           | | | |
| | | +---------------------------+ | | |
| | | +----------+ +----------+     | | |
| | | | embedded | | embedded | ... | | |
| | | +----------+ +----------+     | | |
| | +-------------------------------+ | |
| | +------+ +------+ +------+        | |
| | |attach| |attach| |attach| ...    | |
| | +------+ +------+ +------+        | |
| +-----------------------------------+ |
| +-application/pgp-encrypted or sign-+ |
| | encrypted data or signature       | |
| +-----------------------------------+ |
+---------------------------------------+
*/
MimeMessage *newMimeMessage (dstrbuf *msg)
{
	MimeMultiPart
		*mmp_alternative = NULL,
		*mmp_related = NULL,
		*mmp_mixed = NULL;
	MimeBodyPart *textPart = NULL;
	MimeMessage *mimeMessage = NULL;	// will be one of the above

	dstrbuf *buf = dsbNew (msg->len);
	dsbCopy (buf, msg->str);

	// text/plain (default)
	textPart = newTextPart (CONTENT_SUBTYPE_PLAIN);
	textPart->content = buf;
	textPart->contentHeaders.contentType.paramCharset = Mopts.body_encoding;
	mimeMessage = textPart;

	// text/html
	if (Mopts.html)
	{
		textPart->contentHeaders.contentType.subType = CONTENT_SUBTYPE_HTML;
	}
	// multipart/alternative (will wrap textPart and alternative htmlPart)
	else if (Mopts.alt_html_filename)
	{
		mmp_alternative = newMultipartPart (CONTENT_SUBTYPE_ALTERNATIVE);

		MimeBodyPart *htmlPart = newTextPart (CONTENT_SUBTYPE_HTML);
		dstrbuf *sbHTML = getFileContents (Mopts.alt_html_filename);
		htmlPart->content = sbHTML;

		addBodyPart (mmp_alternative, textPart);
		addBodyPart (mmp_alternative, htmlPart);

		mimeMessage = mmp_alternative;
	}

	// multipart/related (will wrap above mimeMessage and related attachments)
	if (Mopts.related_attach)	// && (Mopts.html || Mopts.alt_html_filename)
	{
		mmp_related = newMultipartPart (CONTENT_SUBTYPE_RELATED);

		addBodyPart (mmp_related, mimeMessage);
		attachFiles (mmp_related, Mopts.related_attach);

		mimeMessage = mmp_related;
	}

	// multipart/mixed (will wrap above mimeMessage and attachments)
	if (Mopts.attach)
	{
		mmp_mixed = newMultipartPart (CONTENT_SUBTYPE_MIXED);

		addBodyPart (mmp_mixed, mimeMessage);
		attachFiles (mmp_mixed, Mopts.attach);

		mimeMessage = mmp_mixed;
	}

	mimeMessage->contentHeaders.contentTransferEncoding = Mopts.transfer_encoding;

	// PGP: multipart/pgp-encrypted, multipart/pgp-signed
	// S/MIME: multipart/pkcs7-encrypted, multipart/pkcs7-signed
	// (will encrypt or sign the above mimeMessage, then wrap it and controlPart)
	// we can encrypt the entire MIME message or separated into different parts
	if (Mopts.gpg_opts)
	{
		MimeBodyPart *controlPart = newApplicationPart (CONTENT_SUBTYPE_PGP_ENCRYPTED);
		MimeBodyPart *dataPart = NULL;
		dstrbuf *sbControl = DSB_NEW,
				*sbSecure = NULL,
				*sbMessage = DSB_NEW;

		printMimeMultiPart (mimeMessage, sbMessage, false, true);
#ifdef _DEBUG
		printf ("Contents to be encrypted/signed:\n[");
		printf (sbMessage->str);
		printf ("]\n");
#endif
		sbSecure = callGpg (sbMessage, Mopts.gpg_opts);
		if (!sbSecure)
		{
		}


		if (Mopts.gpg_opts & GPG_ENC)
		{
			mmp_mixed = newMultipartPart (CONTENT_SUBTYPE_ENCRYPTED);
			mmp_mixed->contentHeaders.contentType.paramProtocol = xstrdup("application/pgp-encrypted");

			dataPart = newApplicationPart (CONTENT_SUBTYPE_OCTET_STREAM);

			controlPart->content = sbControl;
			dataPart->content = sbSecure;
			dsbDestroy (sbMessage);

			//addHeader (controlPart, "Version", "1", false, false);
			dsbCat (sbControl, "Version: 1\r\n");

			dataPart->contentHeaders.contentType.paramName      = xstrdup("encrypted.asc");

			addBodyPart (mmp_mixed, controlPart);
			addBodyPart (mmp_mixed, dataPart);
		}
		else //if (Mopts.gpg_opts & GPG_SIG)
		{
			mmp_mixed = newMultipartPart (CONTENT_SUBTYPE_SIGNED);
			mmp_mixed->contentHeaders.contentType.paramProtocol = xstrdup("application/pgp-signature");
			mmp_mixed->contentHeaders.contentType.paramMicalg   = xstrdup("pgp-sha1");

			dataPart = mimeMessage;
			controlPart->content = sbSecure;
			dsbDestroy (sbControl);

			controlPart->contentHeaders.contentType.subType     = CONTENT_SUBTYPE_PGP_SIGNATURE;
			controlPart->contentHeaders.contentDescription      = xstrdup("This is a digitally signed message");

			addBodyPart (mmp_mixed, dataPart);
			addBodyPart (mmp_mixed, controlPart);
		}

		mimeMessage = mmp_mixed;
	}
	return mimeMessage;
}
MimeMultiPart *newPart (const char *primaryType, const char *subType)
{
	MimeMultiPart *part = xmalloc(sizeof(MimeMultiPart));
	memset (part, 0, sizeof(MimeMultiPart));
	part->contentHeaders.contentType.primaryType = primaryType;
	part->contentHeaders.contentType.subType = subType;
	return part;
}
MimeMultiPart *newTextPart (const char *subType)
{
	return newPart (CONTENT_PRIMARY_TYPE_TEXT, subType);
}
MimeMultiPart *newMultipartPart (const char *subType)
{
	MimeMultiPart *part = newPart (CONTENT_PRIMARY_TYPE_MULTIPART, subType);
	dstrbuf *sbBoundary = mimeMakeBoundary();
	part->isMultiPartContent = true;
	part->contentHeaders.contentType.paramBoundary = xstrdup(sbBoundary->str);
	dsbDestroy (sbBoundary);
	return part;
}
MimeMultiPart *newApplicationPart (const char *subType)
{
	return newPart (CONTENT_PRIMARY_TYPE_APPLICATION, subType);
}
extern void defaultDestr(void *ptr);
void addBodyPart (MimeMultiPart *mmp, MimeBodyPart *mbp)
{
	dlist bodyParts = mmp->content;
	if (! bodyParts) bodyParts = dlInit(defaultDestr);	// TODO: should define a destroyer
	dlAppend (bodyParts, mbp);
	mmp->content = bodyParts;
	mbp->parent = mmp;
}

int attachFiles (MimeMultiPart *mmp, dlist filenames)
{
	char *filename;
	dstrbuf *file_name = NULL;
	dstrbuf *file_type = NULL;
	struct stat file_stat;
	int rc;
	while ((filename = dlGetNext(filenames)) != NULL)
	{
		FILE *fp = fopen(filename, "r");
		if (!fp) {
			fatal("Could not open related attachment: %s", filename);
			return (ERROR);
		}

		/* If the user specified an absolute path, just get the file name */
		file_type = mimeFiletype(filename);
		file_name = mimeFilename(filename);

		memset (&file_stat, 0, sizeof(struct stat));
		rc = stat (filename, &file_stat);
		MimeBodyPart *attachment = newPart (NULL, NULL);
		attachment->contentHeaders.contentType.contentType = xstrdup (file_type->str);
		//attachment->contentHeaders.contentType.primaryType = CONTENT_TYPE_TEXT;
		//attachment->contentHeaders.contentType.subType = CONTENT_SUBTYPE_HTML;
		attachment->contentHeaders.contentType.paramName = xstrdup (file_name->str);	//filename;
		//attachment->contentHeaders.contentID;	// TODO: refer by cid: URL in HTML body part
		//attachment->contentHeaders.contentLocation;
		//attachment->contentHeaders.contentBase;
		attachment->contentHeaders.contentTransferEncoding = CONTENT_TRANSFER_ENCODING_BASE64;
		attachment->contentHeaders.isEncoded = true;
		attachment->contentHeaders.contentDisposition.dispositionType = CONTENT_DISPOSITION_ATTACHMENT;
		attachment->contentHeaders.contentDisposition.paramFilename = xstrdup (file_name->str);
		attachment->contentHeaders.contentLocation = xstrdup (file_name->str);
		if (rc==0)
		{
			//attachment->contentHeaders.contentDisposition.paramCreationTime = ;	// file_stat.st_ctim
			//attachment->contentHeaders.contentDisposition.paramModificationTime = ;	// file_stat.st_mtim
			//attachment->contentHeaders.contentDisposition.paramAccessTime = ;	// file_stat.st_atim
			dstrbuf *buf = DSB_NEW;
			dsbPrintf (buf, "%d", file_stat.st_size);
			attachment->contentHeaders.contentDisposition.paramSize = xstrdup(buf->str);
			dsbDestroy (buf);
		}

		dsbDestroy(file_type);
		dsbDestroy(file_name);

		dstrbuf *buf;
		if (rc==0)
			buf = dsbNew (file_stat.st_size * 4 / 3);	// BASE64 size : original = 4:3
		else
			buf = DSB_NEW;

		mimeB64EncodeFile (fp, buf);
		attachment->content = buf;
		addBodyPart (mmp, attachment);
		fclose (fp);
	}
	return SUCCESS;
}

void addHeader (MimeMultiPart *mmp, const char *name, const char *value, bool bNeedToBeQuoted, bool bNeedToBeEncoded)
{
	assert (mmp);
	dlist headers = mmp->otherHeaders;
	if (! headers)
	{
		headers = dlInit (headerDestroyer);
		mmp->otherHeaders = headers;
	}

	Header *header = xmalloc (sizeof(Header));
	memset (header, 0, sizeof(Header));
	header->name = xstrdup (name);
	header->value = xstrdup (value);
	//header->parameters = NULL;	// otherHeaders don't have parameters
	header->needToBeQuoted = bNeedToBeQuoted;
	header->needToBeEncoded = bNeedToBeEncoded;
	dlAppend (headers, header);
}

/*
params:
	mmp: MimeMultiPart
	buf: Output string buffer
	bPrintMailHeader: Print mail header or not (only be 'true' when printing whole mail message)
    bPrintForSecurity: Print for security or not.
        It's only be 'true' when making secure mail (encrypt or signature).
        If it's true, we'll NOT print CRLF after the last boundary string ("--boundary--")
          to assure the data integrity.
*/
void printMimeMultiPart (MimeMultiPart *mmp, dstrbuf *buf, bool bPrintMailHeader, bool bPrintForSecurity)
{
	assert (mmp);
	// Headers begin
	//if (! mmp->parent)
	if (bPrintMailHeader)
	{
		printMailHeaders (buf);
	}
	// Content Headers
	printContentHeaders (mmp->contentHeaders, buf);
	dsbCat (buf, "\r\n");
	// Headers end

	if (mmp->isMultiPartContent)
	{
		MimeMultiPart *bodyPart;
		while ((bodyPart = (MimeMultiPart *)dlGetNext(mmp->content)) != NULL) {
			// print boundary
			dsbPrintf (buf, "--%s\r\n", mmp->contentHeaders.contentType.paramBoundary);

			// print body part
			printMimeMultiPart (bodyPart, buf, false, false);
		}
		dsbPrintf (buf, "--%s--", mmp->contentHeaders.contentType.paramBoundary);
		if (! bPrintForSecurity)
			dsbCat (buf, "\r\n");
	}
	else if (mmp->content)
	{
		dstrbuf *msg = (dstrbuf *)(mmp->content);
		dstrbuf *enc = NULL;
		//dsbCat (buf, msg->str);
		if (mmp->contentHeaders.contentTransferEncoding && !mmp->contentHeaders.isEncoded)
		{
			if (strcasecmp(mmp->contentHeaders.contentTransferEncoding, CONTENT_TRANSFER_ENCODING_QP)==0)
			{
				enc = mimeQpEncodeString((u_char *)msg->str, true);
			}
			else if (strcasecmp(mmp->contentHeaders.contentTransferEncoding, CONTENT_TRANSFER_ENCODING_BASE64)==0)
			{
				enc = mimeB64EncodeString((u_char *)msg->str, msg->len, true);
			}
		}

		if (enc)
			dsbCat (buf, enc->str);
		else
			dsbCat (buf, msg->str);

		dsbDestroy (enc);

		char cLastChar = msg->str[msg->len-1];
		if (cLastChar!='\n' && cLastChar!='\r')
			dsbCat (buf, "\r\n");
	}
}

void printMailHeaders (dstrbuf *buf)
{
	char *subject=Mopts.subject;
	char *user_name = getConfValue("MY_NAME");
	char *email_addr = getConfValue("MY_EMAIL");
	char *sm_bin = getConfValue("SENDMAIL_BIN");
	char *smtp_serv = getConfValue("SMTP_SERVER");
	char *reply_to = getConfValue("REPLY_TO");

	printSubjectHeader (subject, Mopts.encoding, buf);
	printFromHeader  (user_name, email_addr, buf);
	printToHeaders(Mopts.to, Mopts.cc, buf);

	/**
	 * We want to check here to see if we are sending mail by invoking sendmail
	 * If so, We want to add the BCC line to the headers.  Sendmail checks this
	 * Line and makes sure it sends the mail to the BCC people, and then remove
	 * the BCC addresses...  Keep in mind that sending to an smtp servers takes
	 * presidence over sending to sendmail incase both are mentioned.
	 */
	if (sm_bin && !smtp_serv) {
		printBccHeader(Mopts.bcc, buf);
	}

	/* The rest of the standard headers */
	printDateHeader(buf);
	if (reply_to) {
		dsbPrintf(buf, "Reply-To: <%s>\r\n", reply_to);
	}
	dsbPrintf(buf, "X-Mailer: Cleancode.email v%s \r\n", EMAIL_VERSION);
	if (Mopts.priority) {
		dsbPrintf(buf, "X-Priority: 1\r\n");
	}
	printExtraHeaders(Mopts.headers, buf);

	// MIME-Version
	dsbCat (buf, "MIME-Version: 1.0\r\n");
}

void printSubjectHeader (char *subject, bool encoding, dstrbuf *buf)
{
	dstrbuf *dsb = NULL;
	if (subject) {
		if (encoding) {
			dsb = detectCharSetAndEncode (subject);
			if (dsb) subject = dsb->str;
		}
		dsbPrintf(buf, "Subject: %s\r\n", subject);
		dsbDestroy(dsb);
	}
}

void printRecipientsHeaders(const char *type, dlist receipients, dstrbuf *msg)
{
	if (receipients == NULL) return;

	struct addr *a = (struct addr *)dlGetNext(receipients);
	dsbPrintf (msg, "%s: ", type);
	while (a != NULL)
	{
		dstrbuf *tmp = formatEmailAddr(a->name, a->email);
		dsbPrintf(msg, "%s", tmp->str);
		dsbDestroy(tmp);
		a = (struct addr *)dlGetNext(receipients);
		if (a != NULL) {
			dsbCat (msg, ", ");
		} else {
			dsbCat (msg, "\r\n");
		}
	}
}

/**
 * just prints the to headers, and cc headers if available
**/
void printToHeaders (dlist to, dlist cc, dstrbuf *msg)
{
	printRecipientsHeaders ("To", to, msg);
	printRecipientsHeaders ("Cc", cc, msg);
}

/**
 * Bcc gets a special function because it's not always printed for standard
 * Mail delivery.  Only if sendmail is going to be invoked, shall it be printed
 * reason being is because sendmail needs the Bcc header to know everyone who
 * is going to recieve the message, when it is done with reading the Bcc headers
 * it will remove this from the headers field.
**/
void printBccHeader (dlist bcc, dstrbuf *msg)
{
	printRecipientsHeaders ("Bcc", bcc, msg);
}

/** Print From Headers **/
void printFromHeader (char *name, char *address, dstrbuf *msg)
{
	dstrbuf *addr = formatEmailAddr(name, address);
	dsbPrintf(msg, "From: %s\r\n", addr->str);
	dsbDestroy(addr);
}

/** Print Date Headers **/
void printDateHeader (dstrbuf *msg)
{
	time_t set_time;
	struct tm *lt;
	char buf[MAXBUF] = { 0 };

	set_time = time(&set_time);

#ifdef USE_GMT
	lt = gmtime(&set_time);
#else
	lt = localtime(&set_time);
#endif

#ifdef USE_GNU_STRFTIME
	strftime(buf, MAXBUF, "%a, %d %b %Y %H:%M:%S %z", lt);
#else
	strftime(buf, MAXBUF, "%a, %d %b %Y %H:%M:%S %Z", lt);
#endif

	dsbPrintf(msg, "Date: %s\r\n", buf);
}

void printExtraHeaders (dlist headers, dstrbuf *msg)
{
	char *hdr=NULL;
	while ((hdr = (char *)dlGetNext(headers)) != NULL) {
		dsbPrintf(msg, "%s\r\n", hdr);
	}
}

void printContentHeaders (ContentHeaders headers, dstrbuf *buf)
{
	// Content-Type and parameters
	if (headers.contentType.baseType)
		dsbPrintf (buf, "Content-Type: %s", headers.contentType.baseType);
	else if (headers.contentType.primaryType)
		dsbPrintf (buf, "Content-Type: %s/%s", headers.contentType.primaryType, headers.contentType.subType);

	if (headers.contentType.paramBoundary)
		dsbPrintf (buf, "; boundary=\"%s\"", headers.contentType.paramBoundary);
	if (headers.contentType.paramCharset)
		dsbPrintf (buf, "; charset=%s", headers.contentType.paramCharset);
	if (headers.contentType.paramProtocol)
		dsbPrintf (buf, "; protocol=\"%s\"", headers.contentType.paramProtocol);
	if (headers.contentType.paramMicalg)
		dsbPrintf (buf, "; micalg=\"%s\"", headers.contentType.paramMicalg);
	if (headers.contentType.paramName)
		dsbPrintf (buf, "; name=\"%s\"", headers.contentType.paramName);
	if (headers.contentType.paramStart)
		dsbPrintf (buf, "; start=\"%s\"", headers.contentType.paramStart);
	dsbCat (buf, "\r\n");

	// Content-Transfer-Encoding
	if (headers.contentTransferEncoding)
		dsbPrintf (buf, "Content-Transfer-Encoding: %s\r\n", headers.contentTransferEncoding);

	// Content-Disposition and parameters
	if (headers.contentDisposition.dispositionType)
	{
		dsbPrintf (buf, "Content-Disposition: %s", headers.contentDisposition.dispositionType);

		if (headers.contentDisposition.paramFilename)
			dsbPrintf (buf, "; filename=\"%s\"", headers.contentDisposition.paramFilename);
		if (headers.contentDisposition.paramCreationTime)
			dsbPrintf (buf, "; creation-date=\"%s\"", headers.contentDisposition.paramCreationTime);
		if (headers.contentDisposition.paramModificationTime)
			dsbPrintf (buf, "; modification-date=\"%s\"", headers.contentDisposition.paramModificationTime);
		if (headers.contentDisposition.paramAccessTime)
			dsbPrintf (buf, "; read-date=\"%s\"", headers.contentDisposition.paramAccessTime);
		if (headers.contentDisposition.paramSize)
			dsbPrintf (buf, "; size=\"%s\"", headers.contentDisposition.paramSize);

		dsbCat (buf, "\r\n");
	}
	// Content-ID
	if (headers.contentID)
		dsbPrintf (buf, "Content-ID: <%s>\r\n", headers.contentID);
	// Content-Language
	if (headers.contentLanguage)
		dsbPrintf (buf, "Content-Language: %s\r\n", headers.contentLanguage);
	// Content-Description
	if (headers.contentDescription)
		dsbPrintf (buf, "Content-Description: %s\r\n", headers.contentDescription);
	// Content-MD5
	if (headers.contentMD5)
		dsbPrintf (buf, "Content-MD5: %s\r\n", headers.contentMD5);
	// Content-Location
	if (headers.contentLocation)
		dsbPrintf (buf, "Content-Location: %s\r\n", headers.contentLocation);
	// Content-Base
	if (headers.contentBase)
		dsbPrintf (buf, "Content-Base: %s\r\n", headers.contentBase);
}

void printHeaders (dlist headers, dstrbuf *buf)
{
	Header *header;
	while ((header = dlGetNext(headers)) != NULL)
	{
		printHeader (header, buf);
	}
}
void printHeader (Header *header, dstrbuf *buf)
{
	dsbPrintf (buf, "%s: \"%s\"", header->name, header->value);
	char *param;
	while ((param = dlGetNext(header->parameters)) != NULL)
	{
		dsbPrintf (buf, "; %s", param);
	}
	dsbCat (buf, "\r\n");
}

/*
According to http://tools.ietf.org/html/rfc5321#section-4.5.2,
 if 1st character of a line is '.', it should be duplicated.
Data transparency is only needed in plain content (text or HTML) and quoted-printable content,
GPG content, BASE64 content needn't it, because normally there're no '.' in it.

Data transparency SHOULD be processed when sending email data, but it's better
 processed here when "writing" mail, because not all data need to be transparency, as mentioned above.

Params:
    dst: Destination string buffer
    src: The whole source message. Use this function on separated messages is not recommended.
*/
int provisionForDataTransparency (dstrbuf *dst, const char *src)
{
	int nLen = strlen (src);
	const char *pLeft = src, *pRight = src;
	bool bNewLine = true;
	// Sample source string: ".aaa\n.bbb.c\ncc.ddd"
	for (pRight=src; pRight-pLeft<=nLen; pRight++)
	{
		if (*pRight=='\r' || *pRight=='\n')
		{
			bNewLine = true;
			continue;
		}

		if (*pRight=='.' && bNewLine)
		{
			dsbnCat(dst, pLeft, pRight-pLeft+1);	// "." and "aaa\n."
			dsbCatChar (dst, '.');
			pRight ++;
			pLeft = pRight;
		}
		bNewLine = false;
	}
	dsbnCat(dst, pLeft, pRight-pLeft+1);	// the remaining: "bbb.c\ncc.ddd"
	// Sample result string: "..aaa\n..bbb.c\ncc.ddd"
	return SUCCESS;
}

void deleteMimeMessage (MimeMessage *mm)
{
	deleteMimeMultiPart (mm);
}

void deleteMimeMultiPart (MimeMultiPart *mmp)
{
	assert (mmp);
	// Headers begin
	if (! mmp->parent)
	{
		//deleteMailHeaders (mmp);
	}
	// Content Headers
	deleteContentHeaders (mmp->contentHeaders);

	if (mmp->otherHeaders)
		dlDestroy (mmp->otherHeaders);
	// Headers end

	if (mmp->isMultiPartContent)
	{
		MimeMultiPart *bodyPart;
		while ((bodyPart = (MimeMultiPart *)dlGetNext(mmp->content)) != NULL) {
			deleteMimeMultiPart (bodyPart);
		}
	}
	else if (mmp->content)
	{
		dstrbuf *msg = (dstrbuf *)(mmp->content);
		dsbDestroy (msg);
	}

	xfree (mmp);
}

void deleteContentHeaders (ContentHeaders headers)
{
	// Content-Type and parameters
	xfree (headers.contentType.baseType);
	xfree (headers.contentType.paramBoundary);
	xfree (headers.contentType.paramCharset);
	xfree (headers.contentType.paramProtocol);
	xfree (headers.contentType.paramMicalg);
	xfree (headers.contentType.paramName);
	xfree (headers.contentType.paramStart);
	xfree (headers.contentType.paramType);
	xfree (headers.contentType.paramID);

	// Content-Transfer-Encoding
	//xfree (headers.contentTransferEncoding);

	// Content-Disposition and parameters
	if (headers.contentDisposition.dispositionType)
	{
		//xfree (headers.contentDisposition.dispositionType);

		xfree (headers.contentDisposition.paramFilename);
		xfree (headers.contentDisposition.paramCreationTime);
		xfree (headers.contentDisposition.paramModificationTime);
		xfree (headers.contentDisposition.paramAccessTime);
		xfree (headers.contentDisposition.paramSize);
	}
	xfree (headers.contentID);
	xfree (headers.contentLanguage);
	xfree (headers.contentDescription);
	xfree (headers.contentMD5);
	xfree (headers.contentLocation);
	xfree (headers.contentBase);
}

void headerDestroyer (void *header)
{
	if (!header) return;

	Header *h = (Header *)header;
	xfree (h->name);
	xfree (h->value);
	xfree (h);
}
