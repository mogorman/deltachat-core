/*******************************************************************************
 *
 *                              Delta Chat Core
 *                      Copyright (C) 2017 Björn Petersen
 *                   Contact: r10s@b44t.com, http://b44t.com
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see http://www.gnu.org/licenses/ .
 *
 ******************************************************************************/


#ifndef __MRMAILBOX_H__
#define __MRMAILBOX_H__
#ifdef __cplusplus
extern "C" {
#endif


#define MR_VERSION_MAJOR    0
#define MR_VERSION_MINOR    9
#define MR_VERSION_REVISION 7


/**
 * @mainpage Tutorial
 *
 * This document describes how to handle the Delta Chat core library.
 * For general information about Delta Chat itself, see <https://delta.chat> and <https://github.com/deltachat>.
 *
 * Let's start.
 *
 * First of all, you have to define a function that is called by the library on
 * specific events (eg. when the configuration is done or when fresh messages arrive).
 * Your function should look like the following:
 *
 * ```
 * uintptr_t my_delta_handler(mrmailbox_t* nb, int event, uintptr_t data1, uintptr_t data2)
 * {
 *     return 0; // for unhandled events, it is always safe to return 0
 * }
 * ```
 *
 * After that, you can create and configure a mrmailbox_t object easily as follows:
 *
 * ```
 * mrmailbox_t* mb = mrmailbox_new(my_delta_handler, NULL, NULL);
 *
 * mrmailbox_set_config(mb, "addr",    "alice@delta.chat"); // use some real test credentials here
 * mrmailbox_set_config(mb, "mail_pw", "***");
 *
 * mrmailbox_configure_and_connect(mb);
 * ```
 *
 * If this works, you'll receive the event `MR_EVENT_CONFIGURE_ENDED` with `data1` set to `1` -
 * and you can start sending your first message:
 *
 * ```
 * uint32_t contact_id = mrmailbox_create_contact(mb, "bob@delta.chat"); // use a real testing address here
 * uint32_t chat_id    = mrmailbox_create_chat_by_contact_id(mb, contact_id);
 *
 * mrmailbox_send_text_msg(mb, chat_id, "Hi, here is my first message!");
 * ```
 *
 * Now, go to the testing address (bob) and you should have received a normal email.
 * Answer this email in any email program with "Got it!" and you will get the message from delta as follows:
 *
 * ```
 * carray* msglist = mrmailbox_get_chat_msgs(mb, chat_id, 0, 0);
 * for( size_t i = 0; i < carray_count(msglist); i++ )
 * {
 *     uint32_t msg_id = carray_get_uint32(msglist, i);
 *     mrmsg_t* msg    = mrmailbox_get_msg(mb, msg_id);
 *
 *     printf("message %i: %s\n", i+1, msg->m_text);
 * }
 * ```
 *
 * This will output the following two lines:
 *
 * ```
 * Message 1: Hi, here is my first message!
 * Message 2: Got it!
 * ```
 *
 * I think, you got the idea.  For further reading, please dive into the mrmailbox_t class.  See you.
 */


#include <libetpan/libetpan.h> /* defines uint16_t etc. */
#include "mrchatlist.h"
#include "mrchat.h"
#include "mrmsg.h"
#include "mrcontact.h"
#include "mrpoortext.h"
#include "mrparam.h"

typedef struct mrmailbox_t    mrmailbox_t;
typedef struct mrimap_t       mrimap_t;
typedef struct mrsmtp_t       mrsmtp_t;
typedef struct mrsqlite3_t    mrsqlite3_t;
typedef struct mrjob_t        mrjob_t;
typedef struct mrmimeparser_t mrmimeparser_t;


/**
 * Callback function that should be given to mrmailbox_new().
 *
 * @memberof mrmailbox_t
 *
 * @param mailbox the mailbox object as returned by mrmailbox_new().
 *
 * @param event one of the MR_EVENT_* constants
 *
 * @param data1 depends on the event parameter
 *
 * @param data2 depends on the event parameter
 *
 * @return return 0 unless stated otherwise in the event parameter documentation
 */
typedef uintptr_t (*mrmailboxcb_t) (mrmailbox_t*, int event, uintptr_t data1, uintptr_t data2);


/**
 * An object representing a single mailbox.
 *
 * Each mailbox is linked to an IMAP/POP3 account and uses a separate
 * SQLite database for offline functionality and for mailbox-related
 * settings.
 */
typedef struct mrmailbox_t
{
	void*            m_userdata;      /**< the same pointer as given to mrmailbox_new(), may be used by the caller for any purpose */
	char*            m_dbfile;        /**< the database file in file. */
	char*            m_blobdir;       /**< full path of the blob directory in use. */

	/** @privatesection */
	mrsqlite3_t*     m_sql;           /**< Internal SQL object, never NULL */
	mrimap_t*        m_imap;          /**< Internal IMAP object, never NULL */
	mrsmtp_t*        m_smtp;          /**< Internal SMTP object, never NULL */

	pthread_t        m_job_thread;    /**< Internal */
	pthread_cond_t   m_job_cond;      /**< Internal */
	pthread_mutex_t  m_job_condmutex; /**< Internal */
	int              m_job_condflag;  /**< Internal */
	int              m_job_do_exit;   /**< Internal */

	mrmailboxcb_t    m_cb;            /**< Internal */

	char*            m_os_name;       /**< Internal */

	uint32_t         m_cmdline_sel_chat_id; /**< Internal */

	int              m_wake_lock;           /**< Internal */
	pthread_mutex_t  m_wake_lock_critical;  /**< Internal */

	int              m_e2ee_enabled;        /**< Internal */

	#define          MR_LOG_RINGBUF_SIZE 200
	pthread_mutex_t  m_log_ringbuf_critical;  /**< Internal */
	char*            m_log_ringbuf[MR_LOG_RINGBUF_SIZE]; /**< Internal */
	time_t           m_log_ringbuf_times[MR_LOG_RINGBUF_SIZE]; /**< Internal */
	int              m_log_ringbuf_pos; /**< Internal. The oldest position resp. the position that is overwritten next */

} mrmailbox_t;


/* create/open/connect */
mrmailbox_t*    mrmailbox_new               (mrmailboxcb_t, void* userdata, const char* os_name);
void            mrmailbox_unref             (mrmailbox_t*);

int             mrmailbox_open              (mrmailbox_t*, const char* dbfile, const char* blobdir);
void            mrmailbox_close             (mrmailbox_t*);
int             mrmailbox_is_open           (const mrmailbox_t*);

int             mrmailbox_set_config        (mrmailbox_t*, const char* key, const char* value);
char*           mrmailbox_get_config        (mrmailbox_t*, const char* key, const char* def);
int             mrmailbox_set_config_int    (mrmailbox_t*, const char* key, int32_t value);
int32_t         mrmailbox_get_config_int    (mrmailbox_t*, const char* key, int32_t def);
char*           mrmailbox_get_blobdir       (mrmailbox_t*);
char*           mrmailbox_get_version_str   (void);

void            mrmailbox_configure_and_connect(mrmailbox_t*);
void            mrmailbox_configure_cancel  (mrmailbox_t*);
int             mrmailbox_is_configured     (mrmailbox_t*);

void            mrmailbox_connect           (mrmailbox_t*);
void            mrmailbox_disconnect        (mrmailbox_t*);

int             mrmailbox_restore           (mrmailbox_t*, time_t seconds_to_restore); /* not really implemented */
char*           mrmailbox_get_info          (mrmailbox_t*);


/* Handle chatlists */
#define         MR_GCL_ARCHIVED_ONLY        0x01
#define         MR_GCL_NO_SPECIALS          0x02
mrchatlist_t*   mrmailbox_get_chatlist      (mrmailbox_t*, int flags, const char* query);


/* Handle chats */
uint32_t        mrmailbox_create_chat_by_contact_id (mrmailbox_t*, uint32_t contact_id);
uint32_t        mrmailbox_get_chat_id_by_contact_id (mrmailbox_t*, uint32_t contact_id);

uint32_t        mrmailbox_send_text_msg     (mrmailbox_t*, uint32_t chat_id, const char* text_to_send);
uint32_t        mrmailbox_send_msg          (mrmailbox_t*, uint32_t chat_id, mrmsg_t*);
void            mrmailbox_set_draft         (mrmailbox_t*, uint32_t chat_id, const char*);

#define         MR_GCM_ADDDAYMARKER         0x01
carray*         mrmailbox_get_chat_msgs     (mrmailbox_t*, uint32_t chat_id, uint32_t flags, uint32_t marker1before);
int             mrmailbox_get_total_msg_count (mrmailbox_t*, uint32_t chat_id);
int             mrmailbox_get_fresh_msg_count (mrmailbox_t*, uint32_t chat_id);
carray*         mrmailbox_get_fresh_msgs    (mrmailbox_t*);
void            mrmailbox_marknoticed_chat  (mrmailbox_t*, uint32_t chat_id);
carray*         mrmailbox_get_chat_media    (mrmailbox_t*, uint32_t chat_id, int msg_type, int or_msg_type);
uint32_t        mrmailbox_get_next_media    (mrmailbox_t*, uint32_t curr_msg_id, int dir);

void            mrmailbox_archive_chat      (mrmailbox_t*, uint32_t chat_id, int archive);
void            mrmailbox_delete_chat       (mrmailbox_t*, uint32_t chat_id);

carray*         mrmailbox_get_chat_contacts (mrmailbox_t*, uint32_t chat_id);
carray*         mrmailbox_search_msgs       (mrmailbox_t*, uint32_t chat_id, const char* query);

mrchat_t*       mrmailbox_get_chat          (mrmailbox_t*, uint32_t chat_id);


/* Handle group chats */
uint32_t        mrmailbox_create_group_chat (mrmailbox_t*, const char* name);
int             mrmailbox_is_contact_in_chat (mrmailbox_t*, uint32_t chat_id, uint32_t contact_id);
int             mrmailbox_add_contact_to_chat (mrmailbox_t*, uint32_t chat_id, uint32_t contact_id);
int             mrmailbox_remove_contact_from_chat (mrmailbox_t*, uint32_t chat_id, uint32_t contact_id);
int             mrmailbox_set_chat_name     (mrmailbox_t*, uint32_t chat_id, const char* name);
int             mrmailbox_set_chat_image    (mrmailbox_t*, uint32_t chat_id, const char* image);


/* Handle messages */
char*           mrmailbox_get_msg_info      (mrmailbox_t*, uint32_t msg_id);
void            mrmailbox_delete_msgs       (mrmailbox_t*, const uint32_t* msg_ids, int msg_cnt);
void            mrmailbox_forward_msgs      (mrmailbox_t*, const uint32_t* msg_ids, int msg_cnt, uint32_t chat_id);
void            mrmailbox_marknoticed_contact (mrmailbox_t*, uint32_t contact_id);
void            mrmailbox_markseen_msgs     (mrmailbox_t*, const uint32_t* msg_ids, int msg_cnt);
void            mrmailbox_star_msgs         (mrmailbox_t*, const uint32_t* msg_ids, int msg_cnt, int star);
mrmsg_t*        mrmailbox_get_msg           (mrmailbox_t*, uint32_t msg_id);



/* Handle contacts */
uint32_t        mrmailbox_create_contact    (mrmailbox_t*, const char* name, const char* addr);
int             mrmailbox_add_address_book  (mrmailbox_t*, const char*);
carray*         mrmailbox_get_known_contacts (mrmailbox_t*, const char* query);
int             mrmailbox_get_blocked_count (mrmailbox_t*);
carray*         mrmailbox_get_blocked_contacts (mrmailbox_t*);
void            mrmailbox_block_contact     (mrmailbox_t*, uint32_t contact_id, int block);
char*           mrmailbox_get_contact_encrinfo (mrmailbox_t*, uint32_t contact_id);
int             mrmailbox_delete_contact    (mrmailbox_t*, uint32_t contact_id);
mrcontact_t*    mrmailbox_get_contact       (mrmailbox_t*, uint32_t contact_id);


/* logging */
void mrmailbox_log_error           (mrmailbox_t*, int code, const char* msg, ...);
void mrmailbox_log_error_if        (int* condition, mrmailbox_t*, int code, const char* msg, ...);
void mrmailbox_log_warning         (mrmailbox_t*, int code, const char* msg, ...);
void mrmailbox_log_info            (mrmailbox_t*, int code, const char* msg, ...);
void mrmailbox_log_vprintf         (mrmailbox_t*, int event, int code, const char* msg, va_list);
int  mrmailbox_get_thread_index    (void);


/* library private */
uint32_t    mrmailbox_send_msg_i__                            (mrmailbox_t*, mrchat_t*, const mrmsg_t*, time_t);
void        mrmailbox_connect_to_imap                         (mrmailbox_t*, mrjob_t*);
void        mrmailbox_wake_lock                               (mrmailbox_t*);
void        mrmailbox_wake_unlock                             (mrmailbox_t*);
int         mrmailbox_poke_eml_file                           (mrmailbox_t*, const char* file);
int         mrmailbox_is_reply_to_known_message__             (mrmailbox_t*, mrmimeparser_t*);
int         mrmailbox_is_reply_to_messenger_message__         (mrmailbox_t*, mrmimeparser_t*);
time_t      mrmailbox_correct_bad_timestamp__                 (mrmailbox_t* ths, uint32_t chat_id, uint32_t from_id, time_t desired_timestamp, int is_fresh_msg);
void        mrmailbox_add_or_lookup_contacts_by_mailbox_list__(mrmailbox_t* ths, struct mailimf_mailbox_list* mb_list, int origin, carray* ids, int* check_self);
void        mrmailbox_add_or_lookup_contacts_by_address_list__(mrmailbox_t* ths, struct mailimf_address_list* adr_list, int origin, carray* ids, int* check_self);
int         mrmailbox_get_archived_count__                    (mrmailbox_t*);
int         mrmailbox_reset_tables                            (mrmailbox_t*, int bits); /* reset tables but leaves server configuration, 1=jobs, 2=e2ee, 8=rest but server config */
size_t      mrmailbox_get_real_contact_cnt__                  (mrmailbox_t*);
uint32_t    mrmailbox_add_or_lookup_contact__                 (mrmailbox_t*, const char* display_name /*can be NULL*/, const char* addr_spec, int origin, int* sth_modified);
int         mrmailbox_get_contact_origin__                    (mrmailbox_t*, uint32_t id, int* ret_blocked);
int         mrmailbox_is_contact_blocked__                    (mrmailbox_t*, uint32_t id);
int         mrmailbox_real_contact_exists__                   (mrmailbox_t*, uint32_t id);
int         mrmailbox_contact_addr_equals__                   (mrmailbox_t*, uint32_t contact_id, const char* other_addr);
void        mrmailbox_scaleup_contact_origin__                (mrmailbox_t*, uint32_t contact_id, int origin);


/* library private: end-to-end-encryption */
#define MR_E2EE_DEFAULT_ENABLED  1
#define MR_MDNS_DEFAULT_ENABLED  1

typedef struct mrmailbox_e2ee_helper_t {
	int   m_encryption_successfull;
	void* m_cdata_to_free;
} mrmailbox_e2ee_helper_t;

void mrmailbox_e2ee_encrypt             (mrmailbox_t*, const clist* recipients_addr, int e2ee_guaranteed, int encrypt_to_self, struct mailmime* in_out_message, mrmailbox_e2ee_helper_t*);
int  mrmailbox_e2ee_decrypt             (mrmailbox_t*, struct mailmime* in_out_message, int* ret_validation_errors); /* returns 1 if sth. was decrypted, 0 in other cases */
void mrmailbox_e2ee_thanks              (mrmailbox_e2ee_helper_t*); /* frees data referenced by "mailmime" but not freed by mailmime_free(). After calling mre2ee_unhelp(), in_out_message cannot be used any longer! */
int  mrmailbox_ensure_secret_key_exists (mrmailbox_t*); /* makes sure, the private key exists, needed only for exporting keys and the case no message was sent before */


/*******************************************************************************
 * Events
 ******************************************************************************/


/* The following events may be passed to the callback given to mrmailbox_new() */


/* Information, should not be reported, can be logged,
data1=0, data2=info string */
#define MR_EVENT_INFO                     100


/* Warning, should not be reported, should be logged
data1=0, data2=warning string */
#define MR_EVENT_WARNING                  300


/* Error, must be reported to the user by a non-disturbing bubble or so.
data1=error code MR_ERR_*, see below, data2=error string */
#define MR_EVENT_ERROR                    400


/* one or more messages changed for some reasons in the database - added or
removed.  For added messages: data1=chat_id, data2=msg_id */
#define MR_EVENT_MSGS_CHANGED             2000


/* For fresh messages from the INBOX, MR_EVENT_INCOMING_MSG is send;
data1=chat_id, data2=msg_id */
#define MR_EVENT_INCOMING_MSG             2005


/* a single message is send successfully (state changed from PENDING/SENDING to
DELIVERED); data1=chat_id, data2=msg_id */
#define MR_EVENT_MSG_DELIVERED            2010


/* a single message is read by the receiver (state changed from DELIVERED to
READ); data1=chat_id, data2=msg_id */
#define MR_EVENT_MSG_READ                 2015


/* group name/image changed or members added/removed */
#define MR_EVENT_CHAT_MODIFIED            2020


/* contact(s) created, renamed, blocked or deleted */
#define MR_EVENT_CONTACTS_CHANGED         2030


/* connection state changed,
data1=0:failed-not-connected, 1:configured-and-connected */
#define MR_EVENT_CONFIGURE_ENDED          2040


/* data1=percent */
#define MR_EVENT_CONFIGURE_PROGRESS       2041


/* mrmailbox_imex() done:
data1=0:failed, 1=success */
#define MR_EVENT_IMEX_ENDED               2050


/* data1=permille */
#define MR_EVENT_IMEX_PROGRESS            2051


/* file written, event may be needed to make the file public to some system
services. data1=file name, data2=mime type */
#define MR_EVENT_IMEX_FILE_WRITTEN        2052


/* The following events are functions that should be provided by the frontends */


/* check, if the system is online currently
ret=0: not online, ret=1: online */
#define MR_EVENT_IS_ONLINE                2080


/* get a string from the frontend, data1=MR_STR_*, ret=string which will be
free()'d by the backend */
#define MR_EVENT_GET_STRING               2091


/* synchronous http/https(!) call, data1=url, ret=content which will be
free()'d by the backend, 0 on errors */
#define MR_EVENT_GET_QUANTITY_STRING      2092


/* synchronous http/https(!) call, data1=url, ret=content which will be free()'d
by the backend, 0 on errors */
#define MR_EVENT_HTTP_GET                 2100

/* acquire wakeLock (data1=1) or release it (data1=0), the backend does not make
nested or unsynchronized calls */
#define MR_EVENT_WAKE_LOCK                2110


/* Error codes */
#define MR_ERR_SELF_NOT_IN_GROUP          1
#define MR_ERR_NONETWORK                  2


/*******************************************************************************
 * Import/export and Tools
 ******************************************************************************/


#define MR_IMEX_CANCEL                      0
#define MR_IMEX_EXPORT_SELF_KEYS            1 /**< param1 is a directory where the keys are written to */
#define MR_IMEX_IMPORT_SELF_KEYS            2 /**< param1 is a directory where the keys are searched in and read from */
#define MR_IMEX_EXPORT_BACKUP              11 /**< param1 is a directory where the backup is written to */
#define MR_IMEX_IMPORT_BACKUP              12 /**< param1 is the file with the backup to import */
#define MR_IMEX_EXPORT_SETUP_MESSAGE       20 /**< param1 is a directory where the setup file is written to */
#define MR_BAK_PREFIX                      "delta-chat"
#define MR_BAK_SUFFIX                      "bak"
void            mrmailbox_imex              (mrmailbox_t*, int what, const char* param1, const char* setup_code);
char*           mrmailbox_imex_has_backup   (mrmailbox_t*, const char* dir);



int             mrmailbox_check_password    (mrmailbox_t*, const char* pw);



char*           mrmailbox_create_setup_code (mrmailbox_t*);



int             mrmailbox_poke_spec         (mrmailbox_t*, const char* spec);


/* The library tries itself to stay alive. For this purpose there is an additional
"heartbeat" thread that checks if the IDLE-thread is up and working. This check is done about every minute.
However, depending on the operating system, this thread may be delayed or stopped, if this is the case you can
force additional checks manually by just calling mrmailbox_heartbeat() about every minute.
If in doubt, call this function too often, not too less :-) */
void            mrmailbox_heartbeat         (mrmailbox_t*);


/* carray tools, already defined are things as
unsigned unt carray_count() */
uint32_t        carray_get_uint32           (carray*, unsigned int index);


/* deprecated functions */
mrchat_t*       mrchatlist_get_chat_by_index (mrchatlist_t*, size_t index); /* deprecated - use mrchatlist_get_chat_id_by_index() */
mrmsg_t*        mrchatlist_get_msg_by_index (mrchatlist_t*, size_t index);  /* deprecated - use mrchatlist_get_msg_id_by_index() */
int             mrchat_set_draft            (mrchat_t*, const char* msg);   /* deprecated - use mrmailbox_set_draft() instead */


/* library-internal */
void          mrmailbox_unarchive_chat__             (mrmailbox_t*, uint32_t chat_id);
size_t        mrmailbox_get_chat_cnt__               (mrmailbox_t*);
uint32_t      mrmailbox_create_or_lookup_nchat_by_contact_id__(mrmailbox_t*, uint32_t contact_id);
uint32_t      mrmailbox_lookup_real_nchat_by_contact_id__(mrmailbox_t*, uint32_t contact_id);
int           mrmailbox_get_total_msg_count__        (mrmailbox_t*, uint32_t chat_id);
int           mrmailbox_get_fresh_msg_count__        (mrmailbox_t*, uint32_t chat_id);
uint32_t      mrmailbox_get_last_deaddrop_fresh_msg__(mrmailbox_t*);
void          mrmailbox_send_msg_to_smtp             (mrmailbox_t*, mrjob_t*);
void          mrmailbox_send_msg_to_imap             (mrmailbox_t*, mrjob_t*);
int           mrmailbox_add_contact_to_chat__        (mrmailbox_t*, uint32_t chat_id, uint32_t contact_id);
int           mrmailbox_is_contact_in_chat__         (mrmailbox_t*, uint32_t chat_id, uint32_t contact_id);
int           mrmailbox_get_chat_contact_count__     (mrmailbox_t*, uint32_t chat_id);
int           mrmailbox_group_explicitly_left__      (mrmailbox_t*, const char* grpid);
void          mrmailbox_set_group_explicitly_left__  (mrmailbox_t*, const char* grpid);

size_t       mrmailbox_get_real_msg_cnt__     (mrmailbox_t*); /* the number of messages assigned to real chat (!=deaddrop, !=trash) */
size_t       mrmailbox_get_deaddrop_msg_cnt__ (mrmailbox_t*);
int          mrmailbox_rfc724_mid_cnt__       (mrmailbox_t*, const char* rfc724_mid);
int          mrmailbox_rfc724_mid_exists__    (mrmailbox_t*, const char* rfc724_mid, char** ret_server_folder, uint32_t* ret_server_uid);
void         mrmailbox_update_server_uid__    (mrmailbox_t*, const char* rfc724_mid, const char* server_folder, uint32_t server_uid);
void         mrmailbox_update_msg_chat_id__   (mrmailbox_t*, uint32_t msg_id, uint32_t chat_id);
void         mrmailbox_update_msg_state__     (mrmailbox_t*, uint32_t msg_id, int state);
void         mrmailbox_delete_msg_on_imap     (mrmailbox_t* mailbox, mrjob_t* job);
int          mrmailbox_mdn_from_ext__         (mrmailbox_t*, uint32_t from_id, const char* rfc724_mid, uint32_t* ret_chat_id, uint32_t* ret_msg_id); /* returns 1 if an event should be send */
void         mrmailbox_send_mdn               (mrmailbox_t*, mrjob_t* job);
void         mrmailbox_markseen_msg_on_imap   (mrmailbox_t* mailbox, mrjob_t* job);
void         mrmailbox_markseen_mdn_on_imap   (mrmailbox_t* mailbox, mrjob_t* job);


#ifdef __cplusplus
} /* /extern "C" */
#endif
#endif /* __MRMAILBOX_H__ */
