/**

    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2004 email by Dean Jones
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
#ifndef __SMTP_H
#define __SMTP_H   1

typedef struct _Header
{
	char *name;
	char *value;
	bool needToBeQuoted;	// if true, value need to be quoted
	bool needToBeEncoded;	// if true, encode the value using encodeWord function (=?utf-8?B?XXX?=)
	dlist parameters;
} Header, Parameter;

// ContentType with common parameters defined, other parameters are not supported
typedef struct _ContentType
{
	union{
		char *contentType;
		char *baseType;
	};

	union{
		const char *type;	// text, multipart, application, image, audio, video...
		const char *primaryType;
	};
	const char *subType;	// plain, html, mixed, related, alternative,...

	// parameters
	char *paramCharset;	// for text type
	char *paramBoundary;	// for multipart type
	char *paramType;	// for multipart/related
	char *paramStart;	// for multipart/related
	char *paramName;
	char *paramProtocol;	// for 'encryped' and 'signed' sub type (pgp-encrypted or pkcs7-mime or pgp-signed or pkcs7-signature)
	char *paramMicalg;	// Message Integrity Check (MIC) algorithms, for signed sub type (pgp-signed or pkcs7-signature)

	char *paramID;	// for message/partial
	const int paramNumber;	// for message/partial
	const int paramTotal;	// for message/partial
} ContentType;

typedef struct _ContentDisposition
{
	const char *dispositionType;

	char *paramFilename;
	char *paramCreationTime;
	char *paramModificationTime;
	char *paramAccessTime;
	char *paramSize;
} ContentDisposition;

typedef struct _ContentHeaders
{
	ContentType contentType;
	ContentDisposition contentDisposition;
	const char *contentTransferEncoding;
	bool isEncoded;	// whether or not the content has been encoded using contentTransferEncoding
	char *contentLanguage;
	char *contentID;
	char *contentDescription;
	char *contentMD5;
	char *contentLocation;
	char *contentBase;
} ContentHeaders;

/*
 MimeMessage/MimeBodyPart/MimeMultiPart all-in-one for simple implementation.
 Structure name inspired by JavaMail API.
 */
typedef struct _MimeMultiPart
{
	void *parent;
	ContentHeaders contentHeaders;
	dlist otherHeaders;
	bool isMultiPartContent;	// when true, content will be dlist
								// when false, content will be dstrbuf
	union
	{
		void *content;
		void *body;
	};
} MimeMultiPart, MimeMessage, MimeBodyPart;

void createMail(void);

MimeMessage *newMimeMessage (dstrbuf *msg);
MimeMultiPart *newPart (const char *primaryType, const char *subType);
MimeMultiPart *newTextPart (const char *subType);
MimeMultiPart *newMultipartPart (const char *subType);
MimeMultiPart *newApplicationPart (const char *subType);
void addBodyPart (MimeMultiPart *mpp, MimeBodyPart *mbp);
int attachFiles (MimeMultiPart *mpp, dlist filenames);

void printMimeMultiPart (MimeMultiPart *mmp, dstrbuf *buf, bool bPrintMailHeader, bool bPrintForSecurity);
void printMailHeaders (dstrbuf *buf);
void printSubjectHeader (char *subject, bool encoding, dstrbuf *buf);
void printRecipientsHeaders(const char *type, dlist receipients, dstrbuf *msg);
void printToHeaders (dlist to, dlist cc, dstrbuf *msg);
void printBccHeader (dlist bcc, dstrbuf *msg);
void printFromHeader (char *name, char *address, dstrbuf *msg);
void printDateHeader (dstrbuf *msg);
void printExtraHeaders (dlist headers, dstrbuf *msg);
void printContentHeaders (ContentHeaders headers, dstrbuf *buf);
void printHeaders (dlist headers, dstrbuf *buf);
void printHeader (Header *header, dstrbuf *buf);
int provisionForDataTransparency (dstrbuf *dst, const char *src);

void deleteMimeMessage (MimeMessage *mm);
void deleteMimeMultiPart (MimeMultiPart *mmp);
void deleteContentHeaders (ContentHeaders headers);
void headerDestroyer (void *header);

#endif /* __SMTP_H */
