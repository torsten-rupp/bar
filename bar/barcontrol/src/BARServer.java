/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BARServer.java,v $
* $Revision: 1.22 $
* $Author: torsten $
* Contents: BAR server communication functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;

import java.math.BigInteger;

import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.NoRouteToHostException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;

import java.security.interfaces.RSAPublicKey;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.spec.RSAPublicKeySpec;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NullCipher;

import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

/****************************** Classes ********************************/

/** connection error
 */
class ConnectionError extends Error
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new communication error
   * @param message message
   */
  ConnectionError(String message)
  {
    super(message);
  }
}

/** communication error
 */
class CommunicationError extends Error
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new communication error
   * @param message message
   */
  CommunicationError(String message)
  {
    super(message);
  }
}

/** busy indicator
 */
class BusyIndicator
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** called when busy
   * @param n progress value
   */
  public void busy(long n)
  {
  }

  /** check if aborted
   * @return true iff operation aborted
   */
  public boolean isAborted()
  {
    return false;
  }
}

/** process result
 */
abstract class ProcessResult
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  abstract public void process(String result);
}

/** BAR command
 */
class Command
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  public  ProcessResult      processResult;
  public  long               id;                // unique command id
  public  int                errorCode;         // error code
  public  String             errorText;         // error text
  public  boolean            completedFlag;     // true iff command completed
  public  LinkedList<String> result;            // result

  private long               timeoutTimestamp;  // timeout timestamp [ms]

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new command
   * @param id command id
   * @param timeout timeout or 0 [ms]
   * @param processResult process result handler
   */
  Command(long id, int timeout, ProcessResult processResult)
  {
    this.id               = id;
    this.errorCode        = -1;
    this.errorText        = "";
    this.completedFlag    = false;
    this.processResult    = processResult;
    this.result           = new LinkedList<String>();
    this.timeoutTimestamp = (timeout != 0) ? System.currentTimeMillis()+timeout : 0L;
  }

  /** create new command
   * @param id command id
   * @param timeout timeout or 0 [ms]
   */
  Command(long id, int timeout)
  {
    this(id,timeout,null);
  }

  /** create new command
   * @param id command id
   */
  Command(long id)
  {
    this(id,0);
  }

  /** check if end of data
   * @return true iff command completed and all data processed
   */
  public synchronized boolean endOfData()
  {
    if (result.size() == 0)
    {
      if (!completedFlag)
      {
        try
        {
          this.wait();
        }
        catch (InterruptedException exception)
        {
          // ignored
        }
      }
      return (result.size() == 0) && completedFlag;
    }
    else
    {
      return false;
    }
  }

  /** check if completed
   * @return true iff command completed
   */
  public boolean isCompleted()
  {
    return completedFlag;
  }

  /** wait until command completed
   * @param timeout timeout [ms]
   * @return true if command completed, false otherwise
   */
  public synchronized boolean waitForResult(long timeout)
  {
    while (   !completedFlag
           && (result.size() == 0)
           && (timeout != 0)
         )
    {
      if ((timeoutTimestamp == 0L) || (System.currentTimeMillis() < timeoutTimestamp))
      {
        // wait for result
        try
        {
          if (timeout > 0)
          {
            this.wait(timeout);
            timeout = 0;
          }
          else
          {
            this.wait();
          }
        }
        catch (InterruptedException exception)
        {
          // ignored
        }
      }
      else
      {
        // overall timeout
        timeout();
      }
    }

    return completedFlag || (result.size() > 0);
  }

  /** wait until commmand completed
   */
  public synchronized boolean waitForResult()
  {
    return waitForResult(-1);
  }

  /** wait until command completed
   * @param timeout timeout [ms]
   * @return true if command completed, false otherwise
   */
  public boolean waitCompleted(long timeout)
  {
    boolean timeoutFlag = false;
    synchronized(this)
    {
      while (   !completedFlag
             && (timeout != 0)
             && !timeoutFlag
            )
      {
        if ((timeoutTimestamp == 0L) || (System.currentTimeMillis() < timeoutTimestamp))
        {
          // wait for result
          try
          {
            if (timeout > 0)
            {
              this.wait(timeout);
              timeout = 0;
            }
            else
            {
              this.wait();
            }
          }
          catch (InterruptedException exception)
          {
            // ignored
          }
        }
        else
        {
          // overall timeout (Note: do not handle here, because of syncronization)
          timeoutFlag = true;
        }
      }
    }

    // check if timeout
    if (timeoutFlag)
    {
      timeout();
    }

    return completedFlag;
  }

  /** wait until commmand completed
   */
  public synchronized void waitCompleted()
  {
    waitCompleted(-1);
  }

  /** get error code
   * @return error code
   */
  public synchronized int getErrorCode()
  {
    return errorCode;
  }

  /** get error text
   * @return error text
   */
  public synchronized String getErrorText()
  {
    return errorText;
  }

  /** get next resultg
   * @param timeout timeout [ms]
   * @return result string or null
   */
  public synchronized String getNextResult(long timeout)
  {
    if (   !completedFlag
        && (result.size() == 0)
       )
    {
      try
      {
        this.wait(timeout);
      }
      catch (InterruptedException exception)
      {
        // ignored
      }
    }

    return (result.size() > 0) ? result.removeFirst() : null;
  }

  /** get next result
   * @return result string or null
   */
  public synchronized String getNextResult()
  {
    return (result.size() > 0) ? result.removeFirst() : null;
  }

  /** get result string array
   * @param result result string array to fill
   * @return error code
   */
  public synchronized int getResult(String result[])
  {
    if (errorCode == Errors.NONE)
    {
      result[0] = (this.result.size() > 0) ? this.result.removeFirst() : "";
    }
    else
    {
      result[0] = this.errorText;
    }

    return errorCode;
  }

  /** get result string list array
   * @param result string list array
   * @return error code
   */
  public synchronized int getResult(ArrayList<String> result)
  {
    if (errorCode == Errors.NONE)
    {
      result.clear();
      result.addAll(this.result);
    }
    else
    {
      result.add(this.errorText);
    }

    return errorCode;
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @param timeout timeout or -1 [ms]
   * @return error code
   */
  public synchronized int getNextResult(TypeMap typeMap, String[] errorMessage, ValueMap valueMap, ValueMap unknownValueMap, int timeout)
  {
    // init variables
    if (unknownValueMap != null) unknownValueMap.clear();

    if (errorCode == Errors.NONE)
    {
      // wait for result
      waitForResult(timeout);

      // parse next line
      if (result.size() > 0)
      {
        String line = result.removeFirst();
        if (!line.isEmpty())
        {
          valueMap.clear();
          if (!StringParser.parse(line,typeMap,valueMap,unknownValueMap))
          {
            throw new RuntimeException("parse '"+line+"' fail");
          }
//Dprintf.dprintf("line=%s",line);
//Dprintf.dprintf("typeMap=%s",typeMap);
//Dprintf.dprintf("valueMap=%s",valueMap);
        }
      }
    }

    // get error message
    if (errorMessage != null) errorMessage[0] = errorText;

    return errorCode;
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @return error code
   */
  public synchronized int getResult(TypeMap typeMap, String[] errorMessage, ValueMap valueMap, ValueMap unknownValueMap)
  {
    return getNextResult(typeMap,errorMessage,valueMap,unknownValueMap,0);
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @param timeout timeout or -1 [ms]
   * @return error code
   */
  public synchronized int getNextResult(TypeMap typeMap, String[] errorMessage, ValueMap valueMap, int timeout)
  {
    return getNextResult(typeMap,errorMessage,valueMap,(ValueMap)null,timeout);
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @return error code
   */
  public synchronized int getNextResult(TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(typeMap,errorMessage,valueMap,0);
  }


  /** get result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @return error code
   */
  public synchronized int getResult(TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(typeMap,errorMessage,valueMap);
  }

  /** get result list
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @return error code
   */
  public synchronized int getResult(TypeMap typeMap, String[] errorMessage, List<ValueMap> valueMapList, ValueMap unknownValueMap)
  {
    valueMapList.clear();
    if (unknownValueMap != null) unknownValueMap.clear();

    if (errorCode == Errors.NONE)
    {
      for (String line : result)
      {
        if (!line.isEmpty())
        {
          ValueMap valueMap = new ValueMap();
          StringParser.parse(line,typeMap,valueMap,unknownValueMap);
//Dprintf.dprintf("line=%s",line);
//Dprintf.dprintf("typeMap=%s",typeMap);
//Dprintf.dprintf("valueMap=%s",valueMap);
          valueMapList.add(valueMap);
        }
      }
      if (errorMessage != null) errorMessage[0] = "";
    }
    else
    {
      if (errorMessage != null) errorMessage[0] = this.errorText;
    }

    return errorCode;
  }

  /** get result string list array
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMapList value map list
   * @return error code
   */
  public synchronized int getResult(TypeMap typeMap, String[] errorMessage, List<ValueMap> valueMapList)
  {
    return getResult(typeMap,errorMessage,valueMapList,(ValueMap)null);
  }

  /** get error code
   * @return error code
   */
  public synchronized int getResult()
  {
    return errorCode;
  }

  /** abort command
   */
  public void abort()
  {
    BARServer.abortCommand(this);
  }

  /** timeout command
   */
  public void timeout()
  {
    BARServer.timeoutCommand(this);
  }

  /** convert data to string
   * @return string
   */
  public String toString()
  {
    return "Command {"+id+", errorCode="+errorCode+", error="+errorText+", completedFlag="+completedFlag+"}";
  }
}

/** BAR command handler
 */
abstract class CommandHandler
{
  /** handle command result
   * @param command command
   * @return Errors.NONE or error code
   */
  abstract public int handleResult(Command command);
}

abstract class CommandResultHandler
{
  /** handle command result
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  abstract public int handleResult(ValueMap valueMap);
}

class ResultTypes extends HashMap<String,Class>
{
}

class Result extends HashMap<String,Object>
{
  Result(String errorText)
  {
    super();
    put("error",errorText);
  }
}

/** server result read thread
 */
class ReadThread extends Thread
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private BufferedReader        input;
  private boolean               quitFlag;
  private HashMap<Long,Command> commandHashMap = new HashMap<Long,Command>();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create read thread
   * @param input input stream
   */
  ReadThread(BufferedReader input)
  {
    this.input = input;
    setDaemon(true);

    quitFlag = false;
  }

  /** run method
   */
  public void run()
  {
    String line;

    while (!quitFlag)
    {
      try
      {
        // next line
        try
        {
          line = input.readLine();
          if (line == null)
          {
            if (!quitFlag)
            {
              throw new IOException("disconnected");
            }
            else
            {
              break;
            }
          }
          if (Settings.debugServerFlag) System.err.println("Network: received '"+line+"'");

          // parse: line format <id> <error code> <completed flag> <data>
          String parts[] = line.split(" ",4);
          if (parts.length < 4)
          {
            throw new CommunicationError("malformed command result '"+line+"'");
          }

          // get command id, completed flag, error code
          long    commandId     = Long.parseLong(parts[0]);
          boolean completedFlag = (Integer.parseInt(parts[1]) != 0);;
          int     errorCode     = Integer.parseInt(parts[2]);
          String  data          = parts[3].trim();

          // store result
          synchronized(commandHashMap)
          {
            Command command = commandHashMap.get(commandId);
            if (command != null)
            {
              synchronized(command)
              {
                if (completedFlag)
                {
                  command.errorCode = errorCode;
                  if (errorCode == Errors.NONE)
                  {
                    if (command.processResult != null)
                    {
                      command.processResult.process(data);
                    }
                    else
                    {
                      if (!data.isEmpty())
                      {
                        command.result.add(data);
                      }
                    }
                  }
                  else
                  {
                    command.errorText = data;
                  }
                  command.completedFlag = true;
                  command.notifyAll();
                }
                else
                {
                  command.errorCode = Errors.NONE;
                  if (command.processResult != null)
                  {
                    command.processResult.process(data);
                  }
                  else
                  {
                    if (!data.isEmpty())
                    {
                      command.result.add(data);
                      command.notifyAll();
                    }
                  }
                }
              }
            }
            else
            {
              // result for unknown command -> currently ignored
//Dprintf.dprintf("not found %d: %s\n",commandId,line);
            }
          }
        }
        catch (SocketTimeoutException exception)
        {
          // ignored
        }
        catch (NumberFormatException exception)
        {
          // ignored
        }
      }
      catch (IOException exception)
      {
        // communication impossible, cancel all commands with error and wait for termination
        synchronized(commandHashMap)
        {
          while (!quitFlag)
          {
            for (Command command : commandHashMap.values())
            {
              command.errorCode     = Errors.NETWORK_RECEIVE;
              command.errorText     = exception.getMessage();
              command.completedFlag = true;
            }

            try { commandHashMap.wait(); } catch (InterruptedException interruptedException) { /* ignored */ }
          }
        }
      }
    }
  }

  /** quit thread
   */
  public void quit()
  {
    quitFlag = true;
    interrupt();
  }

  /** add command
   * @param commandId command id
   * @param timeout timeout or 0 [ms]
   * @param processResult process result handler
   * @return command
   */
  public Command commandAdd(long commandId, int timeout, ProcessResult processResult)
  {
    synchronized(commandHashMap)
    {
      Command command = new Command(commandId,timeout,processResult);
      commandHashMap.put(commandId,command);
      commandHashMap.notifyAll();
      return command;
    }
  }

  /** add command
   * @param commandId command id
   * @param timeout timeout or 0 [ms]
   * @return command
   */
  public Command commandAdd(long commandId, int timeout)
  {
    return commandAdd(commandId,timeout,null);
  }

  /** add command
   * @param commandId command id
   * @return command
   */
  public Command commandAdd(long commandId)
  {
    return commandAdd(commandId,0);
  }

  /** remove command
   * @param command command to remove
   */
  public int commandRemove(Command command)
  {
    synchronized(commandHashMap)
    {
      commandHashMap.remove(command.id);
      commandHashMap.notifyAll();
      return command.errorCode;
    }
  }
}

/** BAR server
 */
class BARServer
{
  // --------------------------- constants --------------------------------
  private final static int   PROTOCOL_VERSION_MAJOR = 2;
  private final static int   PROTOCOL_VERSION_MINOR = 0;

  public final static String JAVA_SSL_KEY_FILE_NAME = "bar.jks";  // default name Java TLS/SSL key

  public static char         fileSeparator;

  private final static int   SOCKET_READ_TIMEOUT    = 20*1000;    // timeout reading socket [ms]

  private static byte[]      RANDOM_DATA = new byte[64];

  // --------------------------- variables --------------------------------
  private static byte[]         sessionId;
  private static String         passwordEncryptType;
  private static Cipher         passwordCipher;
  private static Key            passwordKey;

  private static long           commandId;
  private static Socket         socket;
  private static BufferedWriter output;
  private static BufferedReader input;
  private static ReadThread     readThread;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  // init random data
  static
  {
    Random random = new Random(System.currentTimeMillis());
    random.nextBytes(RANDOM_DATA);
  }

  /** connect to BAR server
   * @param hostname host name
   * @param port port number or 0
   * @param tlsPort TLS port number of 0
   * @param serverPassword server password
   */
  public static void connect(String hostname, int port, int tlsPort, String serverPassword, String serverKeyFileName)
  {
    final int TIMEOUT = 20;

    assert hostname != null;
    assert (port != 0) || (tlsPort != 0);

    commandId = 0;

    // connect to server
    socket = null;
    String connectErrorMessage = null;
    if ((socket == null) && (tlsPort != 0))
    {
      // get all possible bar.jks file names
      String[] javaSSLKeyFileNames = null;
      if (serverKeyFileName != null)
      {
        javaSSLKeyFileNames = new String[]
        {
          serverKeyFileName
        };
      }
      else
      {
        javaSSLKeyFileNames = new String[]
        {
          JAVA_SSL_KEY_FILE_NAME,
          System.getProperty("user.home")+File.separator+".bar"+File.separator+JAVA_SSL_KEY_FILE_NAME,
          Config.CONFIG_DIR+File.separator+JAVA_SSL_KEY_FILE_NAME,
          Config.TLS_DIR+File.separator+"private"+File.separator+JAVA_SSL_KEY_FILE_NAME
        };
      }

      // try to connect with key
      for (String javaSSLKeyFileName : javaSSLKeyFileNames)
      {
        File file = new File(javaSSLKeyFileName);
        if (file.exists() && file.isFile() && file.canRead())
        {
          System.setProperty("javax.net.ssl.trustStore",javaSSLKeyFileName);
//Dprintf.dprintf("javaSSLKeyFileName=%s\n",javaSSLKeyFileName);
          try
          {
            SSLSocket        sslSocket;
            SSLSocketFactory sslSocketFactory;

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();
            sslSocket        = (SSLSocket)sslSocketFactory.createSocket(hostname,tlsPort);

/*
String[] ss;

ss = sslSocket.getSupportedCipherSuites();
for (String s : ss)
{
Dprintf.dprintf("getSupportedCipherSuites=%s",s);
}
ss = sslSocket.getSupportedProtocols();
for (String s : ss)
{
Dprintf.dprintf("getSupportedProtocols=%s",s);
}
ss = sslSocket.getEnabledProtocols();
for (String s : ss)
{
Dprintf.dprintf("getEnabledProtocols=%s",s);
}
//sslSocket.setEnabledCipherSuites(new String[]{"SSL_RSA_WITH_RC4_128_MD5","SSL_RSA_WITH_RC4_128_SHA","TLS_RSA_WITH_AES_128_CBC_SHA"," TLS_DHE_RSA_WITH_AES_128_CBC_SHA"," TLS_DHE_DSS_WITH_AES_128_CBC_SHA"," SSL_RSA_WITH_3DES_EDE_CBC_SHA"," SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA"," SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA"," SSL_RSA_WITH_DES_CBC_SHA"," SSL_DHE_RSA_WITH_DES_CBC_SHA"," SSL_DHE_DSS_WITH_DES_CBC_SHA"," SSL_RSA_EXPORT_WITH_RC4_40_MD5"," SSL_RSA_EXPORT_WITH_DES40_CBC_SHA","SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA","SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"});
sslSocket.setEnabledCipherSuites(new String[]{"SSL_RSA_WITH_3DES_EDE_CBC_SHA","SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA","SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA","SSL_RSA_WITH_DES_CBC_SHA","SSL_DHE_RSA_WITH_DES_CBC_SHA","SSL_DHE_DSS_WITH_DES_CBC_SHA","SSL_RSA_EXPORT_WITH_RC4_40_MD5","SSL_RSA_EXPORT_WITH_DES40_CBC_SHA","SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA","SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"});
sslSocket.setEnabledProtocols(new String[]{"SSLv3"});
*/
            sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
            sslSocket.startHandshake();

//java.security.cert.Certificate[] serverCerts = sslSocket.getSession().getPeerCertificates();
//Dprintf.dprintf("serverCerts=%s\n",serverCerts);

            input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream()));

            socket = sslSocket;
            break;
          }
          catch (SocketTimeoutException exception)
          {
            connectErrorMessage = "host '"+hostname+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            connectErrorMessage = "host '"+hostname+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            connectErrorMessage = "host '"+hostname+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            connectErrorMessage = "unknown host '"+hostname+"'";
          }
          catch (RuntimeException exception)
          {
            connectErrorMessage = exception.getMessage();
          }
          catch (Exception exception)
          {
            connectErrorMessage = exception.getMessage();
          }
        }
      }
    }
    if ((socket == null) && (port != 0))
    {
      try
      {
        socket = new Socket(hostname,port);
        socket.setSoTimeout(SOCKET_READ_TIMEOUT);

        input  = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        output = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream()));
      }
      catch (SocketTimeoutException exception)
      {
        connectErrorMessage = exception.getMessage();
      }
      catch (ConnectException exception)
      {
        connectErrorMessage = exception.getMessage();
      }
      catch (NoRouteToHostException exception)
      {
        connectErrorMessage = "host '"+hostname+"' unreachable (no route to host)";
      }
      catch (UnknownHostException exception)
      {
        connectErrorMessage = "unknown host '"+hostname+"'";
      }
      catch (Exception exception)
      {
//        exception.printStackTrace();
        connectErrorMessage = exception.getMessage();
      }
    }
    if (socket == null)
    {
      if   ((tlsPort != 0) || (port!= 0)) throw new ConnectionError(connectErrorMessage);
      else                                throw new ConnectionError("no server ports specified");
    }

    sessionId           = null;
    passwordEncryptType = null;
    passwordCipher      = null;
    try
    {
      String   line;
      String[] errorMessage = new String[1];
      ValueMap valueMap     = new ValueMap();
      String[] data;

      // read session data
      line = input.readLine();
      if (line == null)
      {
        throw new CommunicationError("No result from server");
      }
      if (Settings.debugServerFlag) System.err.println("Network: received '"+line+"'");
      data = line.split(" ",2);
      if ((data.length < 2) || !data[0].equals("SESSION"))
      {
        throw new CommunicationError("Invalid response from server");
      }
      if (!StringParser.parse(data[1],
                              new TypeMap("id",String.class,
                                          "encryptTypes",String.class,
                                          "n",String.class,
                                          "e",String.class
                                         ),
                              valueMap
                             )
         )
      {
        throw new CommunicationError("Invalid response from server");
      }
      sessionId = decodeHex(valueMap.getString("id"));

      // get password chipher
      String[] encryptTypes = valueMap.getString("encryptTypes").split(",");
      int      i            = 0;
      while ((i < encryptTypes.length) && (passwordCipher == null))
      {
        if      (encryptTypes[i].equalsIgnoreCase("RSA"))
        {
          // encrypted passwords with RSA
          try
          {
            BigInteger n = valueMap.containsKey("n") ? new BigInteger(valueMap.getString("n"),16) : null;
            BigInteger e = valueMap.containsKey("e") ? new BigInteger(valueMap.getString("e"),16) : null;
//Dprintf.dprintf("n=%s e=%s",n,e);

            RSAPublicKeySpec rsaPublicKeySpec = new RSAPublicKeySpec(n,e);
            PublicKey        publicKey        = KeyFactory.getInstance("RSA").generatePublic(rsaPublicKeySpec);

            passwordEncryptType = "RSA";
            passwordCipher      = Cipher.getInstance("RSA/ECB/PKCS1Padding");
//            passwordCipher = Cipher.getInstance("RSA/ECB/OAEPWithSHA-1AndMGF1Padding");
//            passwordCipher = Cipher.getInstance("RSA/ECB/OAEPWithSHA-256AndMGF1Padding");
            passwordKey         = publicKey;
          }
          catch (java.security.NoSuchAlgorithmException  exception)
          {
            if (Settings.debugFlag)
            {
              BARControl.printStackTrace(exception);
            }
          }
          catch (javax.crypto.NoSuchPaddingException exception)
          {
            if (Settings.debugFlag)
            {
              BARControl.printStackTrace(exception);
            }
          }
          catch (java.security.spec.InvalidKeySpecException exception)
          {
            if (Settings.debugFlag)
            {
              BARControl.printStackTrace(exception);
            }
          }
        }
        else if (encryptTypes[i].equalsIgnoreCase("NONE"))
        {
          passwordEncryptType = "NONE";
          passwordCipher      = new NullCipher();
          passwordKey         = null;
        }

        i++;
      }
      if (passwordCipher == null)
      {
        throw new CommunicationError("Init password cipher fail");
      }

      // authorize
      if (syncExecuteCommand(StringParser.format("AUTHORIZE encryptType=%s encryptedPassword=%s",
                                                 passwordEncryptType,
                                                 encryptPassword(serverPassword)
                                                ),
                             errorMessage
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Authorization fail");
      }
//System.exit(1);

      // get version
      if (syncExecuteCommand("VERSION",
                             new TypeMap("major",int.class,
                                         "minor",int.class
                                        ),
                             errorMessage,
                             valueMap
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Cannot get protocol version for '"+hostname+"': "+errorMessage[0]);
      }
      if (valueMap.getInt("major") != PROTOCOL_VERSION_MAJOR)
      {
        throw new CommunicationError("Incompatible protocol version for '"+hostname+"': expected "+PROTOCOL_VERSION_MAJOR+", got "+data[3]);
      }
      if (valueMap.getInt("minor") != PROTOCOL_VERSION_MINOR)
      {
        BARControl.printWarning("Incompatible minor protocol version for '"+hostname+"': expected "+PROTOCOL_VERSION_MINOR+", got "+data[4]);
      }

      // get file separator character
      if (syncExecuteCommand("GET name=FILE_SEPARATOR",
                             new TypeMap("value",String.class),
                             errorMessage,
                             valueMap
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Get file separator character fail (error: "+data[3]+")");
      }
      fileSeparator = valueMap.getString("value","/").charAt(0);
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error on "+socket.getInetAddress()+":"+socket.getPort()+": "+exception.getMessage());
    }

    // start read thread
    readThread = new ReadThread(input);
    readThread.start();
  }

  /** disconnect from BAR server
   */
  public static void disconnect()
  {
    try
    {
      // flush data (ignore errors)
      executeCommand("JOB_FLUSH");
//synchronized(output) { output.write("QUIT"); output.write('\n'); output.flush(); }

      // close connection, stop read thread
      readThread.quit();
      socket.close();
      try { readThread.join(); } catch (InterruptedException exception) { /* ignored */ }

      // free resources
      input.close();
      output.close();
    }
    catch (IOException exception)
    {
      // ignored
    }
  }

  /** quit BAR server (for debug only)
   * @param result result
   * @return true if quit command sent, false otherwise
   */
  public static boolean quit(String[] result)
  {
    try
    {
      String line = "QUIT";
      output.write(line); output.write('\n'); output.flush();
      if (Settings.debugServerFlag) System.err.println("Network: sent '"+line+"'");
    }
    catch (IOException exception)
    {
      result[0] = BARControl.reniceIOException(exception).getMessage();
      return false;
    }

    return true;
  }

  /** start running command
   * @param commandString command to start
   * @param processResult process result handler
   * @return command
   */
  public static Command runCommand(String commandString, ProcessResult processResult)
  {
    final int TIMEOUT = 120*1000; // total timeout [ms]

    Command command;

    synchronized(output)
    {
      // new command
      commandId++;
      String line = String.format("%d %s",commandId,commandString);

      // add command
      command = readThread.commandAdd(commandId,TIMEOUT,processResult);

      // send command
      try
      {
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugServerFlag) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
        throw new CommunicationError(exception.getMessage());
      }
    }

    return command;
  }

  /** start running command
   * @param commandString command to start
   * @return command
   */
  public static Command runCommand(String commandString)
  {
    return runCommand(commandString,null);
  }

  /** abort command execution
   * @param command command to abort
   * @param result result (String[] or ArrayList)
   * @return Errors.NONE or error code
   */
  static void abortCommand(Command command)
  {
    // send abort command to command
    executeCommand(StringParser.format("ABORT jobId=%d",command.id));
    readThread.commandRemove(command);

    // set abort error
    command.errorCode     = -2;
    command.errorText     = "aborted";
    command.completedFlag = true;
    command.result.clear();
  }

  /** timeout command execution
   * @param command command to abort
   * @param result result (String[] or ArrayList)
   * @return Errors.NONE or error code
   */
  static void timeoutCommand(Command command)
  {
    // send abort command to command
    executeCommand(StringParser.format("ABORT jobId=%d",command.id));
    readThread.commandRemove(command);

    // set abort error
    command.errorCode     = -3;
    command.errorText     = "timeout";
    command.completedFlag = true;
    command.result.clear();
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param commandResultHandler command result handler
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, final TypeMap typeMap, final String[] errorMessage, final CommandResultHandler commandResultHandler, BusyIndicator busyIndicator)
  {
    return executeCommand(commandString,
                          busyIndicator,
                          new CommandHandler()
    {
      public int handleResult(Command command)
      {
        ValueMap valueMap = new ValueMap();
        int      error    = command.getNextResult(typeMap,errorMessage,valueMap);
        if (error == Errors.NONE)
        {
          error = commandResultHandler.handleResult(valueMap);
        }

        return error;
      }
    });
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param commandResultHandler command result handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, final TypeMap typeMap, final String[] errorMessage, CommandResultHandler commandResultHandler)
  {
    return executeCommand(commandString,typeMap,errorMessage,commandResultHandler,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, final TypeMap typeMap, final String[] errorMessage, final ValueMap valueMap, final ValueMap unknownValueMap, BusyIndicator busyIndicator)
  {
    return executeCommand(commandString,
                          busyIndicator,
                          new CommandHandler()
    {
      public int handleResult(Command command)
      {
//Dprintf.dprintf("handle command=%s",command);
        return command.getResult(typeMap,errorMessage,valueMap,unknownValueMap);
      }
    });
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, TypeMap typeMap, String[] errorMessage, ValueMap valueMap, ValueMap unknownValueMap)
  {
    return executeCommand(commandString,typeMap,errorMessage,valueMap,unknownValueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
  {
    return executeCommand(commandString,typeMap,errorMessage,valueMap,(ValueMap)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String command, String[] errorMessage)
  {
    return executeCommand(command,(TypeMap)null,errorMessage,(ValueMap)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String command)
  {
    return executeCommand(command,(String[])null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, final TypeMap typeMap, final String[] errorMessage, final List<ValueMap> valueMapList, final ValueMap unknownValueMap, BusyIndicator busyIndicator)
  {
    return executeCommand(commandString,
                          busyIndicator,
                          new CommandHandler()
    {
      public int handleResult(Command command)
      {
        return command.getResult(typeMap,errorMessage,valueMapList,unknownValueMap);
      }
    });
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, TypeMap typeMap, String[] errorMessage, List<ValueMap> valueMapList, ValueMap unknownValueMap)
  {
    return executeCommand(commandString,typeMap,errorMessage,valueMapList,unknownValueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param typeMap type map
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, TypeMap typeMap, String[] errorMessage, List<ValueMap> valueMapList)
  {
    return executeCommand(commandString,typeMap,errorMessage,valueMapList,(ValueMap)null);
  }

  /** set boolean value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param b value
   */
  public static void set(String name, boolean b)
  {
    executeCommand(StringParser.format("SET name=%s value=%s",name,b ? "yes" : "no"));
  }

  /** set long value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param n value
   */
  static void set(String name, long n)
  {
    executeCommand(StringParser.format("SET name=%s value=%d",name,n));
  }

  /** set string value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param s value
   */
  public static void set(String name, String s)
  {
    executeCommand(StringParser.format("SET name=% value=%S",name,s));
  }

  /** get boolean value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static boolean getBooleanOption(int jobId, String name)
  {
    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();

    if (executeCommand(StringParser.format("OPTION_GET jobId=%d name=%S",jobId,name),
                       new TypeMap("value",Boolean.class),
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      return resultMap.getBoolean("value");
    }
    else
    {
      return false;
    }
  }

  /** get long value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static long getLongOption(int jobId, String name)
  {
    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();

    if (executeCommand(StringParser.format("OPTION_GET jobId=%d name=%S",jobId,name),
                       new TypeMap("value",Long.class),
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      return resultMap.getLong("value");
    }
    else
    {
      return 0L;
    }
  }

  /** get string value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static String getStringOption(int jobId, String name)
  {
    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();

    if (executeCommand(StringParser.format("OPTION_GET jobId=%d name=%S",jobId,name),
                       new TypeMap("value",String.class),
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      return resultMap.getString("value");
    }
    else
    {
      return "";
    }
  }

  /** set boolean option value on BAR server
   * @param jobId job id
   * @param name option name of value
   * @param b value
   */
  public static void setOption(int jobId, String name, boolean b)
  {
    executeCommand(StringParser.format("OPTION_SET jobId=%d name=%S value=%s",jobId,name,b ? "yes" : "no"));
  }

  /** set long option value on BAR server
   * @param jobId job id
   * @param name option name of value
   * @param n value
   */
  public static void setOption(int jobId, String name, long n)
  {
    executeCommand(StringParser.format("OPTION_SET jobId=%d name=%S value=%d",jobId,name,n));
  }

  /** set string option value on BAR server
   * @param jobId job id
   * @param name option name of value
   * @param s value
   */
  public static void setOption(int jobId, String name, String s)
  {
    executeCommand(StringParser.format("OPTION_SET jobId=%d name=%S value=%S",jobId,name,s));
  }

  /** get password encrypt type
   * @return type
   */
  public static String getPasswordEncryptType()
  {
    return passwordEncryptType;
  }

  /** encrypt password as hex-string
   * @param password password
   * @return hex-string
   */
  public static String encryptPassword(String password)
    throws CommunicationError
  {
    byte[] encryptedPasswordBytes = new byte[0];

    if (password != null)
    {
      // get encoded password (XOR with session id)
      byte[] encodedPassword = new byte[sessionId.length];
      try
      {
        byte[] passwordBytes   = password.getBytes("UTF-8");
        for (int i = 0; i < sessionId.length; i++)
        {
          if (i < passwordBytes.length)
          {
            encodedPassword[i] = (byte)((int)passwordBytes[i] ^ (int)sessionId[i]);
          }
          else
          {
            encodedPassword[i] = sessionId[i];
          }
        }
      }
      catch (UnsupportedEncodingException exception)
      {
        throw new CommunicationError("Password encryption fail");
      }

      // encrypt password
      try
      {
        passwordCipher.init(Cipher.ENCRYPT_MODE,passwordKey);
        encryptedPasswordBytes = passwordCipher.doFinal(encodedPassword);
  //Dprintf.dprintf("encryptedPasswordBytes.length=%d serverPassword.getBytes.length=%d",encryptedPasswordBytes.length,password.getBytes("UTF-8").length);
      }
      catch (InvalidKeyException exception)
      {
        if (Settings.debugFlag)
        {
          BARControl.printStackTrace(exception);
        }
      }
      catch (IllegalBlockSizeException exception)
      {
        if (Settings.debugFlag)
        {
          BARControl.printStackTrace(exception);
        }
      }
      catch (BadPaddingException exception)
      {
        if (Settings.debugFlag)
        {
          BARControl.printStackTrace(exception);
        }
      }
      if (encryptedPasswordBytes == null)
      {
        throw new CommunicationError("Password encryption fail");
      }
    }

    // encode as hex-string
    return encodeHex(encryptedPasswordBytes);
  }

  //-----------------------------------------------------------------------

  /** decode hex string
   * @param s hex string
   * @return bytes
   */
  private static byte[] decodeHex(String s)
  {
    byte data[] = new byte[s.length()/2];
    for (int z = 0; z < s.length()/2; z++)
    {
      data[z] = (byte)Integer.parseInt(s.substring(z*2,z*2+2),16);
    }

    return data;
  }

  /** encode hex string
   * @param data bytes
   * @return hex string
   */
  private static String encodeHex(byte data[])
  {
    StringBuffer stringBuffer = new StringBuffer(data.length*2);
    for (int z = 0; z < data.length; z++)
    {
      stringBuffer.append(String.format("%02x",(int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  /** execute command syncronous
   * @param commandString command string
   * @param typeMap types or null
   * @param errorMessage error message or ""
   * @param valueMap values or null
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(String commandString, TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
    throws IOException
  {
    int errorCode;

    synchronized(output)
    {
      // new command
      commandId++;
      String line = String.format("%d %s",commandId,commandString);

      // send command
      output.write(line); output.write('\n'); output.flush();
      if (Settings.debugServerFlag) System.err.println("Network: sent '"+line+"'");

      // read and parse result
      String[] data;
      do
      {
        // read line
        line = input.readLine();
        if (line == null)
        {
          throw new CommunicationError("No result from server");
        }
        if (Settings.debugServerFlag) System.err.println("Network: received '"+line+"'");

        // parse
        data = line.split(" ",4);
        if (data.length < 3) // at least 3 values: <command id> <complete flag> <error code>
        {
          throw new CommunicationError("Invalid response from server");
        }
      }
      while (Integer.parseInt(data[0]) != commandId);

      // check result
      if (Integer.parseInt(data[1]) != 1)
      {
        throw new CommunicationError("Invalid response from server");
      }

      // get result
      errorCode = Integer.parseInt(data[2]);
      if (errorCode == Errors.NONE)
      {
        if (valueMap != null)
        {
          valueMap.clear();
          if (!StringParser.parse(data[3],typeMap,valueMap))
          {
            throw new CommunicationError("Invalid response from server");
          }
        }
        if (errorMessage != null) errorMessage[0] = "";
      }
      else
      {
        if (errorMessage != null) errorMessage[0] = data[3];
      }
    }

    return errorCode;
  }

  /** execute command syncronous
   * @param commandString command string
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(String commandString, String[] errorMessage)
    throws IOException
  {
    return syncExecuteCommand(commandString,(TypeMap)null,errorMessage,(ValueMap)null);
  }

  /** execute command syncronous
   * @param commandString command string
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(String commandString)
    throws IOException
  {
    return syncExecuteCommand(commandString,(String[])null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param busyIndicator busy indicator or null
   * @param commandHandler command handler
   * @return Errors.NONE or error code
   */
  private static int executeCommand(String commandString, BusyIndicator busyIndicator, CommandHandler commandHandler)
  {
    final int TIMEOUT = 120*1000; // total timeout [ms]

    Command command;
    int     errorCode;

    synchronized(output)
    {
      if (busyIndicator != null)
      {
        busyIndicator.busy(0);
        if (busyIndicator.isAborted()) return -1;
      }

      // new command
      commandId++;
      String line = String.format("%d %s",commandId,commandString);

      // add command
      command = readThread.commandAdd(commandId,TIMEOUT);

      // send command
      try
      {
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugServerFlag) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
        return -1;
      }
      if (busyIndicator != null)
      {
        if (busyIndicator.isAborted())
        {
          abortCommand(command);
          return command.getErrorCode();
        }
      }
    }

    // wait until completed, aborted or timeout
    while (   !command.waitCompleted(250)
           && ((busyIndicator == null) || !busyIndicator.isAborted())
          )
    {
      if (busyIndicator != null)
      {
        busyIndicator.busy(0);
      }
    }
    if (busyIndicator != null)
    {
      if (busyIndicator.isAborted())
      {
        command.abort();
        return command.getErrorCode();
      }
    }

    // get result
    errorCode = commandHandler.handleResult(command);

    // free command
    readThread.commandRemove(command);

    return errorCode;
  }
}

/* end of file */
