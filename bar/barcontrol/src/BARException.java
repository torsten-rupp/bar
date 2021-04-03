import java.lang.System;

class BARException extends Exception
{
  public final static int NONE = 0;
  public final static int INSUFFICIENT_MEMORY = 1;
  public final static int INIT = 2;
  public final static int INVALID_ARGUMENT = 3;
  public final static int CONFIG = 4;
  public final static int NO_WRITABLE_CONFIG = 5;
  public final static int ABORTED = 6;
  public final static int INTERRUPTED = 7;
  public final static int FUNCTION_NOT_SUPPORTED = 8;
  public final static int STILL_NOT_IMPLEMENTED = 9;
  public final static int DAEMON_FAIL = 10;
  public final static int IPC = 11;
  public final static int INIT_PASSWORD = 12;
  public final static int NO_PASSWORD = 13;
  public final static int INVALID_PASSWORD_ = 14;
  public final static int PROCESS = 15;
  public final static int INVALID_PATTERN = 16;
  public final static int NO_HOST_NAME = 17;
  public final static int HOST_NOT_FOUND = 18;
  public final static int CONNECT_FAIL = 19;
  public final static int DISCONNECTED = 20;
  public final static int TOO_MANY_CONNECTIONS = 21;
  public final static int NO_LOGIN_NAME = 22;
  public final static int NO_LOGIN_PASSWORD = 23;
  public final static int NETWORK_SEND = 24;
  public final static int NETWORK_RECEIVE = 25;
  public final static int NETWORK_EXECUTE_FAIL = 26;
  public final static int NETWORK_TIMEOUT_SEND = 27;
  public final static int NETWORK_TIMEOUT_RECEIVE = 28;
  public final static int NO_RESPONSE = 29;
  public final static int INVALID_RESPONSE = 30;
  public final static int INIT_TLS = 31;
  public final static int NO_TLS_CA = 32;
  public final static int NO_TLS_CERTIFICATE = 33;
  public final static int NO_TLS_KEY = 34;
  public final static int INVALID_TLS_CA = 35;
  public final static int INVALID_TLS_CERTIFICATE = 36;
  public final static int TLS_CERTIFICATE_EXPIRED = 37;
  public final static int TLS_CERTIFICATE_NOT_ACTIVE = 38;
  public final static int TLS_HANDSHAKE = 39;
  public final static int INVALID_SSH_SPECIFIER = 40;
  public final static int NO_SSH_PUBLIC_KEY = 41;
  public final static int NO_SSH_PRIVATE_KEY = 42;
  public final static int NO_SSH_PASSWORD = 43;
  public final static int INVALID_SSH_PASSWORD = 44;
  public final static int INVALID_SSH_PRIVATE_KEY = 45;
  public final static int SSH_SESSION_FAIL = 46;
  public final static int SSH_AUTHENTICATION = 47;
  public final static int SSH = 48;
  public final static int JOB_ALREADY_EXISTS = 49;
  public final static int JOB_RUNNING = 50;
  public final static int CREATE_JOB = 51;
  public final static int DELETE_JOB = 52;
  public final static int RENAME_JOB = 53;
  public final static int INIT_STORAGE = 54;
  public final static int NO_STORAGE_NAME = 55;
  public final static int INVALID_STORAGE = 56;
  public final static int INVALID_FTP_SPECIFIER = 57;
  public final static int FTP_SESSION_FAIL = 58;
  public final static int NO_FTP_PASSWORD = 59;
  public final static int INVALID_FTP_PASSWORD = 60;
  public final static int FTP_AUTHENTICATION = 61;
  public final static int FTP_GET_SIZE = 62;
  public final static int INVALID_WEBDAV_SPECIFIER = 63;
  public final static int WEBDAV_SESSION_FAIL = 64;
  public final static int NO_WEBDAV_PASSWORD = 65;
  public final static int INVALID_WEBDAV_PASSWORD = 66;
  public final static int WEBDAV_AUTHENTICATION = 67;
  public final static int WEBDAV_BAD_REQUEST = 68;
  public final static int WEBDAV_GET_SIZE = 69;
  public final static int WEBDAV_UPLOAD = 70;
  public final static int WEBDAV_DOWNLOAD = 71;
  public final static int WEBDAV_FAIL = 72;
  public final static int INIT_COMPRESS = 73;
  public final static int INIT_DECOMPRESS = 74;
  public final static int DEFLATE_FAIL = 75;
  public final static int INFLATE_FAIL = 76;
  public final static int COMPRESS_EOF = 77;
  public final static int DELTA_SOURCE_NOT_FOUND = 78;
  public final static int INVALID_COMPRESS_ALGORITHM = 79;
  public final static int UNKNOWN_COMPRESS_ALGORITHM = 80;
  public final static int COMPRESS_ALGORITHM_NOT_SUPPORTED = 81;
  public final static int UNSUPPORTED_BLOCK_LENGTH = 82;
  public final static int INIT_CRYPT = 83;
  public final static int NO_DECRYPT_KEY = 84;
  public final static int NO_CRYPT_PASSWORD = 85;
  public final static int INVALID_CRYPT_PASSWORD = 86;
  public final static int CRYPT_PASSWORDS_MISMATCH = 87;
  public final static int INIT_CIPHER = 88;
  public final static int ENCRYPT_FAIL = 89;
  public final static int DECRYPT_FAIL = 90;
  public final static int CREATE_KEY_FAIL = 91;
  public final static int INVALID_ENCODING = 92;
  public final static int INIT_KEY = 93;
  public final static int KEY_NOT_FOUND = 94;
  public final static int READ_KEY_FAIL = 95;
  public final static int INVALID_KEY = 96;
  public final static int INVALID_KEY_LENGTH = 97;
  public final static int INVALID_BLOCK_LENGTH_ = 98;
  public final static int INVALID_SALT_LENGTH = 99;
  public final static int NO_PUBLIC_CRYPT_KEY = 100;
  public final static int NO_PRIVATE_CRYPT_KEY = 101;
  public final static int NOT_A_PUBLIC_CRYPT_KEY = 102;
  public final static int NOT_A_PRIVATE_CRYPT_KEY = 103;
  public final static int NO_PUBLIC_SIGNATURE_KEY = 104;
  public final static int NO_PRIVATE_SIGNATURE_KEY = 105;
  public final static int NOT_A_PUBLIC_SIGNATURE_KEY = 106;
  public final static int NOT_A_PRIVATE_SIGNATURE_KEY = 107;
  public final static int KEY_ENCRYPT_FAIL = 108;
  public final static int KEY_DECRYPT_FAIL = 109;
  public final static int WRONG_PRIVATE_KEY = 110;
  public final static int INVALID_CRYPT_ALGORITHM = 111;
  public final static int UNKNOWN_CRYPT_ALGORITHM = 112;
  public final static int INIT_HASH = 113;
  public final static int INVALID_HASH_ALGORITHM = 114;
  public final static int UNKNOWN_HASH_ALGORITHM = 115;
  public final static int INIT_MAC = 116;
  public final static int INVALID_MAC_ALGORITHM = 117;
  public final static int UNKNOWN_MAC_ALGORITHM = 118;
  public final static int NO_FILE_NAME = 119;
  public final static int CREATE_FILE = 120;
  public final static int OPEN_FILE = 121;
  public final static int CREATE_DIRECTORY = 122;
  public final static int IO = 123;
  public final static int READ_FILE = 124;
  public final static int END_OF_FILE = 125;
  public final static int WRITE_FILE = 126;
  public final static int DELETE_FILE = 127;
  public final static int OPEN_DIRECTORY = 128;
  public final static int READ_DIRECTORY = 129;
  public final static int FILE_EXISTS_ = 130;
  public final static int FILE_NOT_FOUND_ = 131;
  public final static int FILE_ACCESS_DENIED = 132;
  public final static int NOT_A_DIRECTORY = 133;
  public final static int END_OF_DIRECTORY = 134;
  public final static int INIT_FILE_NOTIFY = 135;
  public final static int INSUFFICIENT_FILE_NOTIFY = 136;
  public final static int OPTICAL_DISK_NOT_FOUND = 137;
  public final static int OPEN_OPTICAL_DISK = 138;
  public final static int OPEN_ISO9660_FILE = 139;
  public final static int NO_DEVICE_NAME = 140;
  public final static int OPEN_DEVICE = 141;
  public final static int INVALID_DEVICE_BLOCK_SIZE = 142;
  public final static int READ_DEVICE = 143;
  public final static int WRITE_DEVICE = 144;
  public final static int PARSE_DEVICE_LIST = 145;
  public final static int MOUNT = 146;
  public final static int UMOUNT = 147;
  public final static int NO_ARCHIVE_FILE_NAME = 148;
  public final static int NOT_AN_ARCHIVE_FILE = 149;
  public final static int ARCHIVE_NOT_FOUND = 150;
  public final static int END_OF_ARCHIVE = 151;
  public final static int NO_META_ENTRY = 152;
  public final static int NO_FILE_ENTRY = 153;
  public final static int NO_FILE_DATA = 154;
  public final static int NO_DIRECTORY_ENTRY = 155;
  public final static int NO_LINK_ENTRY = 156;
  public final static int NO_HARDLINK_ENTRY = 157;
  public final static int NO_SPECIAL_ENTRY = 158;
  public final static int NO_IMAGE_ENTRY = 159;
  public final static int NO_IMAGE_DATA = 160;
  public final static int END_OF_DATA = 161;
  public final static int INCOMPLETE_ARCHIVE = 162;
  public final static int INSUFFICIENT_SPLIT_NUMBERS = 163;
  public final static int CRC_ = 164;
  public final static int ENTRY_NOT_FOUND = 165;
  public final static int ENTRY_INCOMPLETE = 166;
  public final static int WRONG_ENTRY_TYPE = 167;
  public final static int ENTRIES_DIFFER = 168;
  public final static int CORRUPT_DATA = 169;
  public final static int INVALID_CHUNK_SIZE = 170;
  public final static int UNKNOWN_CHUNK = 171;
  public final static int INVALID_SIGNATURE = 172;
  public final static int NOT_AN_INCREMENTAL_FILE = 173;
  public final static int WRONG_INCREMENTAL_FILE_VERSION = 174;
  public final static int CORRUPT_INCREMENTAL_FILE = 175;
  public final static int INVALID_DEVICE_SPECIFIER = 176;
  public final static int LOAD_VOLUME_FAIL = 177;
  public final static int PARSE_COMMAND = 178;
  public final static int EXPAND_TEMPLATE = 179;
  public final static int FORK_FAIL = 180;
  public final static int IO_REDIRECT_FAIL = 181;
  public final static int EXEC_FAIL = 182;
  public final static int EXEC_TERMINATE = 183;
  public final static int PARSE = 184;
  public final static int PARSE_DATE = 185;
  public final static int PARSE_TIME = 186;
  public final static int PARSE_WEEKDAYS = 187;
  public final static int PARSE_MAINTENANCE = 188;
  public final static int PARSE_SCHEDULE = 189;
  public final static int UNKNOWN_COMMAND = 190;
  public final static int EXPECTED_PARAMETER = 191;
  public final static int UNKNOWN_VALUE = 192;
  public final static int INVALID_VALUE = 193;
  public final static int DEPRECATED_OR_IGNORED_VALUE = 194;
  public final static int AUTHORIZATION = 195;
  public final static int JOB_NOT_FOUND = 196;
  public final static int SCHEDULE_NOT_FOUND = 197;
  public final static int PERSISTENCE_ID_NOT_FOUND = 198;
  public final static int MAINTENANCE_ID_NOT_FOUND = 199;
  public final static int SERVER_ID_NOT_FOUND = 200;
  public final static int ENTRY_ID_NOT_FOUND = 201;
  public final static int PATTERN_ID_NOT_FOUND = 202;
  public final static int MOUNT_ID_NOT_FOUND = 203;
  public final static int DELTA_SOURCE_ID_NOT_FOUND = 204;
  public final static int DATABASE = 205;
  public final static int DATABASE_EXISTS = 206;
  public final static int DATABASE_VERSION_UNKNOWN = 207;
  public final static int DATABASE_MISSING_TABLE = 208;
  public final static int DATABASE_MISSING_COLUMN = 209;
  public final static int DATABASE_OBSOLETE_TABLE = 210;
  public final static int DATABASE_OBSOLETE_COLUMN = 211;
  public final static int DATABASE_TYPE_MISMATCH = 212;
  public final static int DATABASE_CREATE_INDEX = 213;
  public final static int DATABASE_INDEX_NOT_FOUND = 214;
  public final static int DATABASE_INDEX_NOT_READY = 215;
  public final static int DATABASE_INVALID_INDEX = 216;
  public final static int DATABASE_PARSE_ID = 217;
  public final static int DATABASE_TIMEOUT = 218;
  public final static int MASTER_DISCONNECTED = 219;
  public final static int SLAVE_DISCONNECTED = 220;
  public final static int NOT_PAIRED = 221;
  public final static int NOT_A_SLAVE = 222;
  public final static int TESTCODE = 223;
  public final static int UNKNOWN = 224;

  public final int    code;
  public final int    errno;
  public final String data;

  /** get error code
   *  error error
   *  error code
   */
  public static int getCode(BARException error)
  {
    return error.code;
  }

  /** get errno
   *  error error
   *  errno
   */
  public static int getErrno(BARException error)
  {
    return error.errno;
  }

  /** get error data
   *  error error
   *  error data
   */
  public static String getData(BARException error)
  {
    return error.data;
  }

  /** get formated error text
   *  error error
   *  formated error text
   */
  public static String getText(int errorCode, int errno, String errorData)
  {
    StringBuilder errorText = new StringBuilder();

    switch (errorCode)
    {
      case UNKNOWN:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown"));
        break;
      case INSUFFICIENT_MEMORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("insufficient memory"));
        break;
      case INIT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize fail"));
        break;
      case INVALID_ARGUMENT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid argument"));
        break;
      case CONFIG:
        stringSet(errorText,sizeof(errorText),BARControl.tr("configuration error"));
        break;
      case NO_WRITABLE_CONFIG:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no writable configuration file"));
        break;
      case ABORTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("aborted"));
        break;
      case INTERRUPTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("interrupted"));
        break;
      case FUNCTION_NOT_SUPPORTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("function not supported"));
        break;
      case STILL_NOT_IMPLEMENTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("function still not implemented"));
        break;
      case DAEMON_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("run as daemon fail"));
        break;
      case IPC:
        stringSet(errorText,sizeof(errorText),BARControl.tr("inter-process communication fail"));
        break;
      case INIT_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize password fail"));
        break;
      case NO_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no password given"));
        break;
      case INVALID_PASSWORD_:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid password"));
        break;
      case PROCESS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("process data fail"));
        break;
      case INVALID_PATTERN:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid pattern"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        break;
      case NO_HOST_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no host name given"));
        break;
      case HOST_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("host not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        break;
      case CONNECT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("connect fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DISCONNECTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("disconnected"));
        break;
      case TOO_MANY_CONNECTIONS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("too many concurrent connections"));
        break;
      case NO_LOGIN_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no login name given"));
        break;
      case NO_LOGIN_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no password given"));
        break;
      case NETWORK_SEND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("sending data fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NETWORK_RECEIVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("receiving data fail"));
        break;
      case NETWORK_EXECUTE_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("execute command fail"));
        break;
      case NETWORK_TIMEOUT_SEND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("timeout send data"));
        break;
      case NETWORK_TIMEOUT_RECEIVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("timeout receive data"));
        break;
      case NO_RESPONSE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no response from remote program"));
        break;
      case INVALID_RESPONSE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid response from remote program"));
        break;
      case INIT_TLS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize TLS (SSL) fail"));
        break;
      case NO_TLS_CA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no TLS (SSL) certificate authority file"));
        break;
      case NO_TLS_CERTIFICATE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no TLS (SSL) certificate file"));
        break;
      case NO_TLS_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no or unreadable TLS (SSL) key file"));
        break;
      case INVALID_TLS_CA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid TLS (SSL) certificate authority"));
        break;
      case INVALID_TLS_CERTIFICATE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid TLS (SSL) certificate"));
        break;
      case TLS_CERTIFICATE_EXPIRED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("TLS (SSL) certificate expired"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case TLS_CERTIFICATE_NOT_ACTIVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("TLS (SSL) certificate is still not active"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case TLS_HANDSHAKE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("TLS (SSL) handshake failure"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_SSH_SPECIFIER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid SSH specifier"));
        break;
      case NO_SSH_PUBLIC_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no SSH public key"));
        break;
      case NO_SSH_PRIVATE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no SSH private key"));
        break;
      case NO_SSH_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no SSH password given"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case INVALID_SSH_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid SSH password"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case INVALID_SSH_PRIVATE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid SSH private key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case SSH_SESSION_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize SSH session fail"));
        break;
      case SSH_AUTHENTICATION:
        stringSet(errorText,sizeof(errorText),BARControl.tr("SSH authentication fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case SSH:
        if (!stringIsEmpty(errorData))
        {
          stringSet(errorText,sizeof(errorText),errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("ssh protocol error"));
        }
        break;
      case JOB_ALREADY_EXISTS:
        stringFormat(errorText,sizeof(errorText),BARControl.tr("job ''%s'' already exists"),errorData);
        break;
      case JOB_RUNNING:
        stringFormat(errorText,sizeof(errorText),BARControl.tr("job ''%s'' running"),errorData);
        break;
      case CREATE_JOB:
        stringSet(errorText,sizeof(errorText),BARControl.tr("create job fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DELETE_JOB:
        stringSet(errorText,sizeof(errorText),BARControl.tr("delete job fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case RENAME_JOB:
        stringSet(errorText,sizeof(errorText),BARControl.tr("rename job fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INIT_STORAGE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize storage fail"));
        break;
      case NO_STORAGE_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no storage name given"));
        break;
      case INVALID_STORAGE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid storage"));
        break;
      case INVALID_FTP_SPECIFIER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid FTP specifier"));
        break;
      case FTP_SESSION_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize FTP session fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NO_FTP_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no FTP password given"));
        break;
      case INVALID_FTP_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid FTP password"));
        break;
      case FTP_AUTHENTICATION:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid FTP user/password"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case FTP_GET_SIZE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("get FTP file size fail"));
        break;
      case INVALID_WEBDAV_SPECIFIER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid WebDAV specifier"));
        break;
      case WEBDAV_SESSION_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize WebDAV session fail"));
        break;
      case NO_WEBDAV_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no WebDAV password given"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case INVALID_WEBDAV_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid WebDAV password"));
        break;
      case WEBDAV_AUTHENTICATION:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid WebDAV user/password"));
        break;
      case WEBDAV_BAD_REQUEST:
        stringSet(errorText,sizeof(errorText),BARControl.tr("WebDAV bad request"));
        break;
      case WEBDAV_GET_SIZE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("get WebDAV file size"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," %s",errorData);
        }
        stringAppend(errorText,sizeof(errorText)," ");
        stringAppend(errorText,sizeof(errorText),BARControl.tr("fail"));
        break;
      case WEBDAV_UPLOAD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("WebDAV upload fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case WEBDAV_DOWNLOAD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("WebDAV download fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case WEBDAV_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("WebDAV fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INIT_COMPRESS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize compress fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INIT_DECOMPRESS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize decompress fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DEFLATE_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("compress fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INFLATE_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("decompress fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case COMPRESS_EOF:
        stringSet(errorText,sizeof(errorText),BARControl.tr("end of compressed file"));
        break;
      case DELTA_SOURCE_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("delta source not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        break;
      case INVALID_COMPRESS_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid compress algorithm"));
        break;
      case UNKNOWN_COMPRESS_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown compress algorithm"));
        break;
      case COMPRESS_ALGORITHM_NOT_SUPPORTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("compress algorithm not supported"));
        break;
      case UNSUPPORTED_BLOCK_LENGTH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unsupported block length"));
        break;
      case INIT_CRYPT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize crypt fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NO_DECRYPT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no decrypt key for cipher"));
        break;
      case NO_CRYPT_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no password given for cipher"));
        break;
      case INVALID_CRYPT_PASSWORD:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid decryption password"));
        break;
      case CRYPT_PASSWORDS_MISMATCH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("encryption passwords mismatch"));
        break;
      case INIT_CIPHER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize cipher fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case ENCRYPT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("encrypt fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DECRYPT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("decrypt fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case CREATE_KEY_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("create public/private key fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_ENCODING:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid encoding"));
        break;
      case INIT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize key fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case KEY_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("public/private key not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case READ_KEY_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("read public/private key fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_KEY_LENGTH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid key length"));
        break;
      case INVALID_BLOCK_LENGTH_:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid block length"));
        break;
      case INVALID_SALT_LENGTH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid salt length"));
        break;
      case NO_PUBLIC_CRYPT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no public encryption key"));
        break;
      case NO_PRIVATE_CRYPT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no private encryption key"));
        break;
      case NOT_A_PUBLIC_CRYPT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key is not a public encryption key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NOT_A_PRIVATE_CRYPT_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key is not a private encryption key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NO_PUBLIC_SIGNATURE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no public signature key"));
        break;
      case NO_PRIVATE_SIGNATURE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no private signature key"));
        break;
      case NOT_A_PUBLIC_SIGNATURE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key is not a public signature key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NOT_A_PRIVATE_SIGNATURE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key is not a private signature key"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case KEY_ENCRYPT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key encryption fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case KEY_DECRYPT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("key decryption fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case WRONG_PRIVATE_KEY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("wrong private decryption key"));
        break;
      case INVALID_CRYPT_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid encryption algorithm"));
        break;
      case UNKNOWN_CRYPT_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown encryption algorithm"));
        break;
      case INIT_HASH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize hash fail"));
        break;
      case INVALID_HASH_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid hash algorithm"));
        break;
      case UNKNOWN_HASH_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown hash algorithm"));
        break;
      case INIT_MAC:
        stringSet(errorText,sizeof(errorText),BARControl.tr("initialize message authentication code fail"));
        break;
      case INVALID_MAC_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid message authentication code algorithm"));
        break;
      case UNKNOWN_MAC_ALGORITHM:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown message authentication code algorithm"));
        break;
      case NO_FILE_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no file name given"));
        break;
      case CREATE_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("create file fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case OPEN_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("open file fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case CREATE_DIRECTORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("create directory fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case IO:
        stringSet(errorText,sizeof(errorText),BARControl.tr("i/o error"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case READ_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("read file fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case END_OF_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("end of file"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case WRITE_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("write file fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DELETE_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("delete file fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case OPEN_DIRECTORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("open directory"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case READ_DIRECTORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("read directory fail"));
        break;
      case FILE_EXISTS_:
        stringSet(errorText,sizeof(errorText),BARControl.tr("file already exists"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case FILE_NOT_FOUND_:
        stringSet(errorText,sizeof(errorText),BARControl.tr("file not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case FILE_ACCESS_DENIED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("access denied"));
        break;
      case NOT_A_DIRECTORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("not a directory"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case END_OF_DIRECTORY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("end of directory list"));
        break;
      case INIT_FILE_NOTIFY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("init file notify fail"));
        break;
      case INSUFFICIENT_FILE_NOTIFY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("insufficient file notify entries"));
        break;
      case OPTICAL_DISK_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("CD/DVD/BD device not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case OPEN_OPTICAL_DISK:
        stringSet(errorText,sizeof(errorText),BARControl.tr("open CD/DVD/BD disk fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case OPEN_ISO9660_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("open ISO 9660 image fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NO_DEVICE_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no device name given"));
        break;
      case OPEN_DEVICE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("open device fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_DEVICE_BLOCK_SIZE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid device block size"));
        break;
      case READ_DEVICE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("read device fail"));
        break;
      case WRITE_DEVICE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("write device fail"));
        break;
      case PARSE_DEVICE_LIST:
        stringSet(errorText,sizeof(errorText),BARControl.tr("error parsing device list"));
        break;
      case MOUNT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("mount fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case UMOUNT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unmount fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case NO_ARCHIVE_FILE_NAME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no archive file name given"));
        break;
      case NOT_AN_ARCHIVE_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("not an archive file"));
        break;
      case ARCHIVE_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("archive not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case END_OF_ARCHIVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("end of archive"));
        break;
      case NO_META_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no meta entry"));
        break;
      case NO_FILE_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no file entry"));
        break;
      case NO_FILE_DATA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no file data entry"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),BARControl.tr(" for ''%s''"),errorData);
        }
        break;
      case NO_DIRECTORY_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no directory entry"));
        break;
      case NO_LINK_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no link entry"));
        break;
      case NO_HARDLINK_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no hard link entry"));
        break;
      case NO_SPECIAL_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no special entry"));
        break;
      case NO_IMAGE_ENTRY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no image entry"));
        break;
      case NO_IMAGE_DATA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("no image data entry"));
        break;
      case END_OF_DATA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("end of data"));
        break;
      case INCOMPLETE_ARCHIVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("incomplete archive"));
        break;
      case INSUFFICIENT_SPLIT_NUMBERS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("insufficient split number digits in name"));
        break;
      case CRC_:
        stringSet(errorText,sizeof(errorText),BARControl.tr("wrong CRC"));
        if (!stringIsEmpty(errorData))
        {
          stringAppend(errorText,sizeof(errorText)," ");
          stringAppend(errorText,sizeof(errorText),BARControl.tr("at offset"));
          stringAppend(errorText,sizeof(errorText)," ");
          stringAppend(errorText,sizeof(errorText),errorData);
        }
        break;
      case ENTRY_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),"entry not found");
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case ENTRY_INCOMPLETE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("entry is incomplete"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case WRONG_ENTRY_TYPE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("wrong entry type"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case ENTRIES_DIFFER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("entries differ"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case CORRUPT_DATA:
        stringSet(errorText,sizeof(errorText),BARControl.tr("corrupt data or invalid password"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_CHUNK_SIZE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid chunk size"));
        break;
      case UNKNOWN_CHUNK:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown chunk"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case INVALID_SIGNATURE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid signature"));
        break;
      case NOT_AN_INCREMENTAL_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid incremental file"));
        break;
      case WRONG_INCREMENTAL_FILE_VERSION:
        stringSet(errorText,sizeof(errorText),BARControl.tr("wrong incremental file version"));
        break;
      case CORRUPT_INCREMENTAL_FILE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("corrupt incremental file"));
        break;
      case INVALID_DEVICE_SPECIFIER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid device specifier"));
        break;
      case LOAD_VOLUME_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("load volume fail"));
        break;
      case PARSE_COMMAND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parse command fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        break;
      case EXPAND_TEMPLATE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("expand command template fail"));
        break;
      case FORK_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("fork for execute external program fail"));
        break;
      case IO_REDIRECT_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("i/o redirect fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case EXEC_FAIL:
        stringSet(errorText,sizeof(errorText),BARControl.tr("execute external program fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        if (errno != 0)
        {
          stringFormatAppend(errorText,sizeof(errorText),", exitcode: %d",errno);
        }
        break;
      case EXEC_TERMINATE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("terminate external program fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": '%s'",errorData);
        }
        if (errno != 0)
        {
          stringFormatAppend(errorText,sizeof(errorText),", signal: %d",errno);
        }
        break;
      case PARSE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing data fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PARSE_DATE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing date fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PARSE_TIME:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing time fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PARSE_WEEKDAYS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing weekdays fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PARSE_MAINTENANCE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing maintenance fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PARSE_SCHEDULE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parsing schedule fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case UNKNOWN_COMMAND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown command"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case EXPECTED_PARAMETER:
        stringSet(errorText,sizeof(errorText),BARControl.tr("expected parameter"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case UNKNOWN_VALUE:
        if (!stringIsEmpty(errorData))
        {
          stringSet(errorText,sizeof(errorText),errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("unknown value"));
        }
        break;
      case INVALID_VALUE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid value"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DEPRECATED_OR_IGNORED_VALUE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("deprecated or ignored value"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case AUTHORIZATION:
        stringSet(errorText,sizeof(errorText),BARControl.tr("authorization fail"));
        break;
      case JOB_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("job not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case SCHEDULE_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("schedule not found"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case PERSISTENCE_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"persistence with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("persistence not found"));
        }
        break;
      case MAINTENANCE_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"maintenance with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("maintenance not found"));
        }
        break;
      case SERVER_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"server with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("server not found"));
        }
        break;
      case ENTRY_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"entry with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("entry not found"));
        }
        break;
      case PATTERN_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"pattern with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("pattern not found"));
        }
        break;
      case MOUNT_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"mount with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("mount not found"));
        }
        break;
      case DELTA_SOURCE_ID_NOT_FOUND:
        if (!stringIsEmpty(errorData))
        {
          stringFormat(errorText,sizeof(errorText),"delta source with id #%s not found",errorData);
        }
        else
        {
          stringSet(errorText,sizeof(errorText),BARControl.tr("delta source not found"));
        }
        break;
      case DATABASE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("database"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DATABASE_EXISTS:
        stringSet(errorText,sizeof(errorText),BARControl.tr("database already exists"));
        break;
      case DATABASE_VERSION_UNKNOWN:
        stringSet(errorText,sizeof(errorText),BARControl.tr("unknown version on index"));
        break;
      case DATABASE_MISSING_TABLE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("missing table"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," '%s'",errorData);
        }
        break;
      case DATABASE_MISSING_COLUMN:
        stringSet(errorText,sizeof(errorText),BARControl.tr("missing column"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," '%s'",errorData);
        }
        break;
      case DATABASE_OBSOLETE_TABLE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("obsolete table"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," '%s'",errorData);
        }
        break;
      case DATABASE_OBSOLETE_COLUMN:
        stringSet(errorText,sizeof(errorText),BARControl.tr("obsolete column"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," '%s'",errorData);
        }
        break;
      case DATABASE_TYPE_MISMATCH:
        stringSet(errorText,sizeof(errorText),BARControl.tr("type mismatch"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," '%s'",errorData);
        }
        break;
      case DATABASE_CREATE_INDEX:
        stringSet(errorText,sizeof(errorText),BARControl.tr("error creating index"));
        break;
      case DATABASE_INDEX_NOT_FOUND:
        stringSet(errorText,sizeof(errorText),BARControl.tr("index not found"));
        break;
      case DATABASE_INDEX_NOT_READY:
        stringSet(errorText,sizeof(errorText),BARControl.tr("index still not initialized"));
        break;
      case DATABASE_INVALID_INDEX:
        stringSet(errorText,sizeof(errorText),BARControl.tr("invalid index"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DATABASE_PARSE_ID:
        stringSet(errorText,sizeof(errorText),BARControl.tr("parse index id fail"));
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText),": %s",errorData);
        }
        break;
      case DATABASE_TIMEOUT:
        stringSet(errorText,sizeof(errorText),BARControl.tr("timeout accessing index"));
        break;
      case MASTER_DISCONNECTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("master disconnected"));
        break;
      case SLAVE_DISCONNECTED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("slave disconnected"));
        break;
      case NOT_PAIRED:
        stringSet(errorText,sizeof(errorText),BARControl.tr("slave is not paired"));
        break;
      case NOT_A_SLAVE:
        stringSet(errorText,sizeof(errorText),BARControl.tr("instance is not a slave"));
        break;
      case TESTCODE:
        stringSet(errorText,sizeof(errorText),"test code");
        if (!stringIsEmpty(errorData))
        {
          stringFormatAppend(errorText,sizeof(errorText)," %s",errorData);
        }
      // end of file
        break;
    }

    return errorText.toString();
  }

  /** get formated error text
   *  error error
   *  formated error text
   */
  public static String getText(BARException error)
  {
    return getText(getCode(error),getErrno(error),getData(error));
  }

  /** create error
   *  errorCode error code
   *  errno errno
   *  errorData error data
   */
  BARException(int errorCode, int errno, String errorData)
  {
    this.code  = errorCode;
    this.errno = errno;
    this.data  = errorData;
  }

  /** create error
   *  errorCode error code
   *  errorData error data
   */
  BARException(int errorCode, String errorData)
  {
    this(errorCode,0,errorData);
  }

  /** create error
   *  errorCode error code
   */
  BARException(int errorCode)
  {
    this(errorCode,(String)null);
  }

  /** create error
   *  errorCode error code
   *  errno errno
   *  format format string
   *  arguments optional arguments
   */
  BARException(int errorCode, int errno, String format, Object... arguments)
  {
    this(errorCode,errno,String.format(format,arguments));
  }

  /** get error message
   *  error message
   */
  public String getMessage()
  {
    return getText(this);
  }

  /** get error code
   *  error code
   */
  public int getCode()
  {
    return code;
  }

  /** get error errno
   *  error errno
   */
  public int getErrno()
  {
    return errno;
  }

  /** get error data
   *  error data
   */
  public String getData()
  {
    return data;
  }

  /** get error text
   *  error text
   */
  public String getText()
  {
    return getText(this);
  }

  /** convert to string
   *  string
   */
  public String toString()
  {
    return getText(this);
  }

  // -------------------------------------------------------------------

  private static int sizeof(StringBuilder buffer)
  {
    return 0;
  }

  private static int sizeof(String buffer)
  {
    return 0;
  }

  private static String strerror(int n)
  {
    return "";
  }

  private static void stringSet(StringBuilder buffer, int size, String text)
  {
    buffer.setLength(0);
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int size, StringBuilder text)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int size, String text)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int n, int size)
  {
    buffer.append(String.format("%03x",n));
  }

  private static void stringFormat(StringBuilder buffer, int size, String format, Object... arguments)
  {
    buffer.setLength(0);
    buffer.append(String.format(format,arguments));
  }

  private static void stringFormatAppend(StringBuilder buffer, int size, String format, Object... arguments)
  {
    buffer.append(String.format(format,arguments));
  }

  private static boolean stringIsEmpty(String string)
  {
    return string.isEmpty();
  }
}
