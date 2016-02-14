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

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.HashMap;
import java.util.HashSet;
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
  public final static int TIMEOUT      = 5*1000;   // default timeout [ms]
  public final static int WAIT_FOREVER = -1;

  // --------------------------- variables --------------------------------
  public  ProcessResult      processResult;
  public  long               id;                // unique command id
  public  int                errorCode;         // error code
  public  String             errorText;         // error text
  public  boolean            completedFlag;     // true iff command completed
  public  ArrayDeque<String> result;            // result
  public  int                debugLevel;        // debug level

  private long               timeoutTimestamp;  // timeout timestamp [ms]

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new command
   * @param id command id
   * @param timeout timeout or 0 [ms]
   * @param debugLevel debug level (0..n)
   * @param processResult process result handler
   */
  Command(long id, int timeout, int debugLevel, ProcessResult processResult)
  {
    this.id               = id;
    this.errorCode        = Errors.UNKNOWN;
    this.errorText        = "";
    this.completedFlag    = false;
    this.processResult    = processResult;
    this.result           = new ArrayDeque<String>();
    this.debugLevel       = debugLevel;
    this.timeoutTimestamp = (timeout != 0) ? System.currentTimeMillis()+timeout : 0L;
  }

  /** create new command
   * @param id command id
   * @param timeout timeout or 0 [ms]
   * @param debugLevel debug level (0..n)
   */
  Command(long id, int timeout, int debugLevel)
  {
    this(id,timeout,debugLevel,(ProcessResult)null);
  }

  /** create new command
   * @param id command id
   * @param debugLevel debug level (0..n)
   */
  Command(long id, int debugLevel)
  {
    this(id,0,debugLevel);
  }

  /** check if end of data
   * @return true iff command completed and all data processed
   */
  public synchronized boolean endOfData()
  {
    while (!completedFlag && (result.size() == 0))
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

    return completedFlag && (result.size() == 0);
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
    return waitForResult(WAIT_FOREVER);
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
    waitCompleted(WAIT_FOREVER);
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
    while (!completedFlag && (result.size() == 0))
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

    return result.pollFirst();
  }

  /** get next result
   * @return result string or null
   */
  public synchronized String getNextResult()
  {
    return result.pollFirst();
  }

  /** get result string array
   * @param result result string array to fill
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String result[])
  {
    if (errorCode == Errors.NONE)
    {
      result[0] = (this.result.size() > 0) ? this.getNextResult() : "";
    }
    else
    {
      result[0] = this.errorText;
    }

    return errorCode;
  }

  /** get result string list array
   * @param result string list array
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(ArrayList<String> result)
  {
    if (errorCode == Errors.NONE)
    {
      result.clear();
      result.addAll(this.result);
      this.result.clear();
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
   * @param timeout timeout or WAIT_FOREVER [ms]
   * @return Errors.NONE or error code
   */
  public synchronized int getNextResult(String[] errorMessage, ValueMap valueMap, ValueMap unknownValueMap, int timeout)
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
        String line = getNextResult();
        if ((valueMap != null) && !line.isEmpty())
        {
          valueMap.clear();
          if (!StringParser.parse(line,valueMap,unknownValueMap))
          {
            throw new RuntimeException("parse '"+line+"' fail");
          }
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
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, ValueMap valueMap, ValueMap unknownValueMap)
  {
    return getNextResult(errorMessage,valueMap,unknownValueMap,0);
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @param timeout timeout or WAIT_FOREVER [ms]
   * @return Errors.NONE or error code
   */
  public synchronized int getNextResult(String[] errorMessage, ValueMap valueMap, int timeout)
  {
    return getNextResult(errorMessage,valueMap,(ValueMap)null,timeout);
  }

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public synchronized int getNextResult(String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(errorMessage,valueMap,0);
  }


  /** get result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(errorMessage,valueMap);
  }

  /** get result list
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, List<ValueMap> valueMapList, ValueMap unknownValueMap)
  {
    if (errorCode == Errors.NONE)
    {
      while (!result.isEmpty())
      {
        String line = getNextResult();
        if (!line.isEmpty())
        {
          ValueMap valueMap = new ValueMap();
          StringParser.parse(line,valueMap,unknownValueMap);
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
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, List<ValueMap> valueMapList)
  {
    return getResult(errorMessage,valueMapList,(ValueMap)null);
  }

  /** get error code
   * @return error code
   */
  public synchronized int getResult()
  {
    return errorCode;
  }

  /** purge all results
   */
  public synchronized void purgeResults()
  {
    String lastResult = result.pollLast();
    result.clear();
    if (lastResult != null) result.add(lastResult);
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
   * @param i result counter [0..n-1]
   * @param command command
   * @return Errors.NONE or error code
   */
  abstract public int handleResult(int i, Command command);
}

abstract class CommandResultHandler
{
  /** handle command result
   * @param i result counter [0..n-1]
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  abstract public int handleResult(int i, ValueMap valueMap);
}

//TODO
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
  private boolean               quitFlag = false;
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
    setName("BARControl Server Read");
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

          // parse: line format <id> <error code> <completed flag> <data>
          String parts[] = line.split(" ",4);
          if (parts.length < 4)
          {
            throw new CommunicationError("malformed command result '"+line+"'");
          }

          // get command id, completed flag, error code
          long    commandId     = Long.parseLong(parts[0]);
          boolean completedFlag = (Integer.parseInt(parts[1]) != 0);
          int     errorCode     = Integer.parseInt(parts[2]);
          String  data          = parts[3].trim();


          // store result
          synchronized(commandHashMap)
          {
            Command command = commandHashMap.get(commandId);
            if (command != null)
            {
              if (Settings.debugLevel > command.debugLevel) System.err.println("Network: received '"+line+"'");

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
              if (Settings.debugLevel > 0) System.err.println("Network: received unknown command result '"+line+"'");
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
    // request quit
    quitFlag = true;

    // interrupt read-commands
    interrupt();
  }

  /** add command
   * @param commandId command id
   * @param timeout timeout or 0 [ms]
   * @param debugLevel debug level (0..n)
   * @param processResult process result handler
   * @return command
   */
  public Command commandAdd(long commandId, int timeout, int debugLevel, ProcessResult processResult)
  {
    Command command = new Command(commandId,timeout,debugLevel,processResult);

    synchronized(commandHashMap)
    {
      commandHashMap.put(commandId,command);
      commandHashMap.notifyAll();
    }

    return command;
  }

  /** add command
   * @param commandId command id
   * @param timeout timeout or 0 [ms]
   * @param debugLevel debug level (0..n)
   * @return command
   */
  public Command commandAdd(long commandId, int timeout, int debugLevel)
  {
    return commandAdd(commandId,timeout,debugLevel,(ProcessResult)null);
  }

  /** add command
   * @param commandId command id
   * @param debugLevel debug level (0..n)
   * @return command
   */
  public Command commandAdd(long commandId, int debugLevel)
  {
    return commandAdd(commandId,0,debugLevel);
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
    }

    return command.errorCode;
  }
}

/** BAR server
 */
class BARServer
{
  // --------------------------- constants --------------------------------
  private final static int   PROTOCOL_VERSION_MAJOR = 4;
  private final static int   PROTOCOL_VERSION_MINOR = 0;

  public final static String JAVA_SSL_KEY_FILE_NAME = "bar.jks";  // default name Java TLS/SSL key

  public static char         fileSeparator;

  private final static int   SOCKET_READ_TIMEOUT    = 20*1000;    // timeout reading socket [ms]

  private static byte[]      RANDOM_DATA = new byte[64];

  /** file types
   */
  enum FileTypes
  {
    FILE,
    DIRECTORY,
    LINK,
    HARDLINK,
    SPECIAL,
    UNKNOWN
  };

  // --------------------------- variables --------------------------------
  private static String         name;
  private static int            port;

  private static byte[]         sessionId;
  private static String         passwordEncryptType;
  private static Cipher         passwordCipher;
  private static Key            passwordKey;

  private static long           commandId = 0;
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
   * @param name host name
   * @param port host port number or 0
   * @param tlsPort TLS port number of 0
   * @param serverPassword server password
   */
  public static void connect(String name, int port, int tlsPort, String serverPassword, String serverKeyFileName)
  {
    final int TIMEOUT = 20;

    Socket         socket = null;
    BufferedWriter output = null;
    BufferedReader input  = null;

    assert name != null;
    assert (port != 0) || (tlsPort != 0);

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

    // connect to server: first try TLS, then plain
    String connectErrorMessage = null;
    if ((socket == null) && (port != 0))
    {
      // try to create TLS socket with plain socket+startSSL
      for (String javaSSLKeyFileName : javaSSLKeyFileNames)
      {
        File file = new File(javaSSLKeyFileName);
        if (file.exists() && file.isFile() && file.canRead())
        {
          System.setProperty("javax.net.ssl.trustStore",javaSSLKeyFileName);
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();

            Socket plainSocket = new Socket(name,port);
            plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

            input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream()));

            // get session
            startSession(input,output);

            // send startSSL, wait for response
            String[] errorMessage = new String[1];
            if (syncExecuteCommand(input,
                                   output,
                                   StringParser.format("START_SSL"),
                                   errorMessage
                                  ) != Errors.NONE
               )
            {
              throw new ConnectionError("Start SSL fail");
            }

            // create TLS socket on plain socket
            sslSocket = (SSLSocket)sslSocketFactory.createSocket(plainSocket,name,tlsPort,false);
            sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
            sslSocket.startHandshake();

            input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream()));

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

//java.security.cert.Certificate[] serverCerts = sslSocket.getSession().getPeerCertificates();
//Dprintf.dprintf("serverCerts=%s\n",serverCerts);

            // connection established => done
            socket = sslSocket;
            break;
          }
          catch (ConnectionError exception)
          {
            connectErrorMessage = exception.getMessage();
          }
          catch (SocketTimeoutException exception)
          {
            connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            connectErrorMessage = "unknown host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (RuntimeException exception)
          {
            connectErrorMessage = exception.getMessage();
          }
          catch (IOException exception)
          {
            connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
          }
          catch (Exception exception)
          {
            connectErrorMessage = exception.getMessage();
          }
        }
      }
    }
    if ((socket == null) && (tlsPort != 0))
    {
      // try to create TLS socket
      for (String javaSSLKeyFileName : javaSSLKeyFileNames)
      {
        File file = new File(javaSSLKeyFileName);
        if (file.exists() && file.isFile() && file.canRead())
        {
          System.setProperty("javax.net.ssl.trustStore",javaSSLKeyFileName);
//Dprintf.dprintf("javaSSLKeyFileName=%s\n",javaSSLKeyFileName);
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();

            // create TLS socket
            sslSocket = (SSLSocket)sslSocketFactory.createSocket(name,tlsPort);
            sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
            sslSocket.startHandshake();

            input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream()));

            // get session
            startSession(input,output);

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

//java.security.cert.Certificate[] serverCerts = sslSocket.getSession().getPeerCertificates();
//Dprintf.dprintf("serverCerts=%s\n",serverCerts);

            // connection established => done
            socket = sslSocket;
            break;
          }
          catch (SocketTimeoutException exception)
          {
            connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            connectErrorMessage = "unknown host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (RuntimeException exception)
          {
            connectErrorMessage = exception.getMessage();
          }
          catch (IOException exception)
          {
            connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
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
        Socket plainSocket = new Socket(name,port);
        plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

        input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream()));
        output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream()));

        startSession(input,output);

        socket = plainSocket;
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
        connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
      }
      catch (UnknownHostException exception)
      {
        connectErrorMessage = "unknown host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
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

    // authorize, get version, file separator
    try
    {
      String[] errorMessage = new String[1];
      ValueMap valueMap     = new ValueMap();

      // authorize
      if (syncExecuteCommand(input,
                             output,
                             StringParser.format("AUTHORIZE encryptType=%s encryptedPassword=%s",
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
      if (syncExecuteCommand(input,
                             output,
                             "VERSION",
                             new TypeMap("major",int.class,
                                         "minor",int.class
                                        ),
                             errorMessage,
                             valueMap
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Cannot get protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': "+errorMessage[0]);
      }
      if (valueMap.getInt("major") != PROTOCOL_VERSION_MAJOR)
      {
        throw new CommunicationError("Incompatible protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': expected "+PROTOCOL_VERSION_MAJOR+", got "+valueMap.getInt("major"));
      }
      if (valueMap.getInt("minor") != PROTOCOL_VERSION_MINOR)
      {
        BARControl.printWarning("Incompatible minor protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': expected "+PROTOCOL_VERSION_MINOR+", got "+valueMap.getInt("minor"));
      }

      // get file separator character
      if (syncExecuteCommand(input,
                             output,
                             "GET name=FILE_SEPARATOR",
                             new TypeMap("value",String.class),
                             errorMessage,
                             valueMap
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Get file separator character fail (error: "+errorMessage+")");
      }
      fileSeparator = valueMap.getString("value","/").charAt(0);
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error on "+socket.getInetAddress()+":"+socket.getPort()+": "+exception.getMessage());
    }

    // disconnect (if connected)
    if (BARServer.socket != null)
    {
      disconnect();
    }

    // setup new connection
    BARServer.name   = name;
    BARServer.port   = socket.getPort();
    BARServer.socket = socket;
    BARServer.input  = input;
    BARServer.output = output;

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
      executeCommand("JOB_FLUSH",0);

      // close connection, stop read thread
      readThread.quit();
      socket.close(); socket = null;
      try { readThread.join(); } catch (InterruptedException exception) { /* ignored */ }; readThread = null;

      // free resources
      input.close(); input = null;
      output.close(); output = null;
    }
    catch (IOException exception)
    {
      // ignored
    }
  }

  /** quit BAR server (for debug only)
   * @return true if quit command sent, false otherwise
   */
  public static boolean quit()
  {
    try
    {
      // flush data (ignore errors)
      executeCommand("JOB_FLUSH",0);

      // send QUIT command
      String[] result = new String[1];
      if (executeCommand("QUIT",0,result) != Errors.NONE)
      {
        return false;
      }

      // sleep a short time
      try { Thread.sleep(1000); } catch (InterruptedException exception) { /* ignored */ }

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

    return true;
  }

  /** Get server info
   * @return server info
   */
  public static String getInfo()
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append(name);
    if (socket instanceof SSLSocket)
    {
      buffer.append(" (TLS)");
    }

    return buffer.toString();
  }

  /** start running command
   * @param commandString command to start
   * @param debugLevel debug level (0..n)
   * @param processResult process result handler
   * @return command
   */
  public static Command runCommand(String commandString, int debugLevel, ProcessResult processResult)
  {
    final int TIMEOUT = 120*1000; // total timeout [ms]

    Command command;

    synchronized(output)
    {
      // add new command
      commandId++;
      command = readThread.commandAdd(commandId,TIMEOUT,debugLevel,processResult);

      // send command
      String line = String.format("%d %s",commandId,commandString);
      try
      {
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugLevel > command.debugLevel) System.err.println("Network: sent '"+line+"'");
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
   * @param debugLevel debug level (0..n)
   * @return command
   */
  public static Command runCommand(String commandString, int debugLevel)
  {
    return runCommand(commandString,debugLevel,(ProcessResult)null);
  }

  /** abort command execution
   * @param command command to abort
   * @param result result (String[] or ArrayList)
   * @return Errors.NONE or error code
   */
  static void abortCommand(Command command)
  {
    // send abort command to command
    executeCommand(StringParser.format("ABORT commandId=%d",command.id),0);
    readThread.commandRemove(command);

    // set abort error
    command.errorCode     = Errors.ABORTED;
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
    executeCommand(StringParser.format("ABORT commandId=%d",command.id),0);
    readThread.commandRemove(command);

    // set timeoout error
    command.errorCode     = Errors.NETWORK_TIMEOUT;
    command.errorText     = "timeout";
    command.completedFlag = true;
    command.result.clear();
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param commandResultHandler command result handler
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                     commandString,
                                   int                        debugLevel,
                                   final String[]             errorMessage,
                                   final CommandResultHandler commandResultHandler,
                                   BusyIndicator              busyIndicator
                                  )
  {
    return executeCommand(commandString,
                          debugLevel,
                          busyIndicator,
                          new CommandHandler()
                          {
                            public int handleResult(int i, Command command)
                            {
                              ValueMap valueMap = new ValueMap();
                              int error = command.getNextResult(errorMessage,valueMap);
                              if (error == Errors.NONE)
                              {
                                error = commandResultHandler.handleResult(i,valueMap);
                              }

                              return error;
                            }
                          }
                         );
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param commandResultHandler command result handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String               commandString,
                                   int                  debugLevel,
                                   final String[]       errorMessage,
                                   CommandResultHandler commandResultHandler
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,commandResultHandler,(BusyIndicator)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param commandResultHandler command result handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String               commandString,
                                   int                  debugLevel,
                                   CommandResultHandler commandResultHandler
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null,commandResultHandler);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String         commandString,
                                   int            debugLevel,
                                   final String[] errorMessage,
                                   final ValueMap valueMap,
                                   final ValueMap unknownValueMap,
                                   BusyIndicator  busyIndicator
                                  )
  {
    return executeCommand(commandString,
                          debugLevel,
                          busyIndicator,
                          new CommandHandler()
    {
      public int handleResult(int i, Command command)
      {
        return command.getResult(errorMessage,valueMap,unknownValueMap);
      }
    });
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @param unknownValueMap unknown values map or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String   commandString,
                                   int      debugLevel,
                                   String[] errorMessage,
                                   ValueMap valueMap,
                                   ValueMap unknownValueMap
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMap,unknownValueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String   commandString,
                                   int      debugLevel,
                                   String[] errorMessage,
                                   ValueMap valueMap
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMap,(ValueMap)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String   commandString,
                                   int      debugLevel,
                                   ValueMap valueMap
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null,valueMap);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String   commandString,
                                   int      debugLevel,
                                   String[] errorMessage
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,(ValueMap)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString,
                                   int    debugLevel
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String               commandString,
                                   int                  debugLevel,
                                   final String[]       errorMessage,
                                   final List<ValueMap> valueMapList,
                                   final ValueMap       unknownValueMap,
                                   BusyIndicator        busyIndicator)
  {
    valueMapList.clear();
    if (unknownValueMap != null) unknownValueMap.clear();

    return executeCommand(commandString,
                          debugLevel,
                          busyIndicator,
                          new CommandHandler()
    {
      public int handleResult(int i, Command command)
      {
        return command.getResult(errorMessage,valueMapList,unknownValueMap);
      }
    });
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @param unknownValueMap unknown values map or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String         commandString,
                                   int            debugLevel,
                                   String[]       errorMessage,
                                   List<ValueMap> valueMapList,
                                   ValueMap       unknownValueMap
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMapList,unknownValueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or ""
   * @param valueMapList value map list
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String         commandString,
                                   int            debugLevel,
                                   String[]       errorMessage,
                                   List<ValueMap> valueMapList
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMapList,(ValueMap)null);
  }

  /** set boolean value on BAR server
   * @param name name of value
   * @param value value
   */
  public static void set(String name, boolean value)
  {
    executeCommand(StringParser.format("SET name=%s value=%s",name,value ? "yes" : "no"),0);
  }

  /** set long value on BAR server
   * @param name name of value
   * @param value value
   */
  static void set(String name, long value)
  {
    executeCommand(StringParser.format("SET name=%s value=%d",name,value),0);
  }

  /** set string value on BAR server
   * @param name name of value
   * @param value value
   */
  public static void set(String name, String value)
  {
    executeCommand(StringParser.format("SET name=% value=%S",name,value),0);
  }

  /** get job option value from BAR server
   * @param jobUUID job UUID
   * @param name name of value
   * @return value
   */
  public static <T> T getJobOption(String jobUUID, String name, Class clazz)
  {
    T data = null;

    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();
    if (executeCommand(StringParser.format("JOB_OPTION_GET jobUUID=%s name=%S",jobUUID,name),
                       0,
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Long.class)
      {
        data = (T)new Long(resultMap.getLong("value"));
      }
      else if (clazz == String.class)
      {
        data = (T)resultMap.get("value");
      }
    }

    return data;
  }

  /** get boolean value from BAR server
   * @param jobUUID job UUID
   * @param name name of value
   * @return value
   */
  public static boolean getBooleanJobOption(String jobUUID, String name)
  {
    return ((Boolean)getJobOption(jobUUID,name,Boolean.class)).booleanValue();
  }

  /** get long value from BAR server
   * @param jobUUID job UUID
   * @param name name of value
   * @return value
   */
  public static long getLongJobOption(String jobUUID, String name)
  {
    return ((Long)getJobOption(jobUUID,name,Long.class)).longValue();
  }

  /** get string value from BAR server
   * @param jobUUID job UUID
   * @param name name of value
   * @return value
   */
  public static String getStringJobOption(String jobUUID, String name)
  {
    return (String)getJobOption(jobUUID,name,String.class);
  }

  /** set boolean job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, boolean value, String errorMessage[])
  {
    return executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                              jobUUID,
                                              name,
                                              value ? "yes" : "no"
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set boolean job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, boolean value)
  {
    return setJobOption(jobUUID,name,value,(String[])null);
  }

  /** set long job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, long value, String errorMessage[])
  {
    return executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%d",
                                              jobUUID,
                                              name,
                                              value
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set long job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, long value)
  {
    return setJobOption(jobUUID,name,value,(String[])null);
  }

  /** set string job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, String value, String errorMessage[])
  {
    return executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%S",
                                              jobUUID,
                                              name,
                                              value
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set string job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, String name, String value)
  {
    return setJobOption(jobUUID,name,value,(String[])null);
  }

  /** get string value from BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   * @param errorMessage error message or ""
   * @return value
   */
  public static int getJobOption(String jobUUID, WidgetVariable widgetVariable, String errorMessage[])
  {
    ValueMap resultMap = new ValueMap();
    int error = executeCommand(StringParser.format("JOB_OPTION_GET jobUUID=%s name=%S",jobUUID,widgetVariable.getName()),
                               0,
                               errorMessage,
                               resultMap
                              );
    if (error !=- Errors.NONE)
    {
      return error;
    }

    if      (widgetVariable.getType() == Boolean.class)
    {
      widgetVariable.set(resultMap.getBoolean("value"));
    }
    else if (widgetVariable.getType() == Long.class)
    {
      widgetVariable.set(resultMap.getLong("value"));
    }
    else if (widgetVariable.getType() == Double.class)
    {
      widgetVariable.set(resultMap.getDouble("value"));
    }
    else if (widgetVariable.getType() == String.class)
    {
      widgetVariable.set(resultMap.getString("value"));
    }
    else if (widgetVariable.getType() == Enum.class)
    {
//        widgetVariable.set(resultMap.getString("value"));
throw new Error("NYI");
    }
    else
    {
      throw new Error("Type not supported");
    }

    return error;
  }

  /** get string value from BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   * @return value
   */
  public static int getJobOption(String jobUUID, WidgetVariable widgetVariable)
  {
    return getJobOption(jobUUID,widgetVariable,(String[])null);
  }

  /** set job option value on BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, WidgetVariable widgetVariable, String errorMessage[])
  {
    int error = Errors.UNKNOWN;

    if      (widgetVariable.getType() == Boolean.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                                 jobUUID,
                                                 widgetVariable.getName(),
                                                 widgetVariable.getBoolean() ? "yes" : "no"
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Long.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%d",
                                                 jobUUID,
                                                 widgetVariable.getName(),
                                                 widgetVariable.getLong()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Double.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%f",
                                                 jobUUID,
                                                 widgetVariable.getName(),
                                                 widgetVariable.getDouble()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == String.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%'S",
                                                 jobUUID,
                                                 widgetVariable.getName(),
                                                 widgetVariable.getString()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Enum.class)
    {
/*
        error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                                   jobUUID,
                                                   widgetVariable.getName(),
                                                   widgetVariable.getLong()
                                                  ),
                               0,
                               errorMessage
                              );
        break;*/
        throw new Error("NYI");
    }
    else
    {
      throw new Error("Type not supported");
    }

    return error;
  }

  /** set job option value on BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   * @return Errors.NONE or error code
   */
  public static int setJobOption(String jobUUID, WidgetVariable widgetVariable)
  {
    return setJobOption(jobUUID,widgetVariable,(String[])null);
  }

  /** get schedule option value from BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name name of value
   * @return value
   */
  public static <T> T getScheduleOption(String jobUUID, String scheduleUUID, String name, Class clazz)
  {
    T data = null;

    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();
    if (executeCommand(StringParser.format("SCHEDULE_OPTION_GET jobUUID=%s scheduleUUID=%s name=%S",jobUUID,scheduleUUID,name),
                       0,
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Long.class)
      {
        data = (T)new Long(resultMap.getLong("value"));
      }
      else if (clazz == String.class)
      {
        data = (T)resultMap.get("value");
      }
    }

    return data;
  }

  /** set boolean schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, boolean value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%s",
                                              jobUUID,
                                              scheduleUUID,
                                              name,
                                              value ? "yes" : "no"
                                             ),
                          0
                         );
  }

  /** set boolean schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, boolean value)
  {
    return setScheduleOption(jobUUID,scheduleUUID,name,value,(String[])null);
  }

  /** set long schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, long value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%d",
                                              jobUUID,
                                              scheduleUUID,
                                              name,
                                              value
                                             ),
                          0
                         );
  }

  /** set long schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, long value)
  {
    return setScheduleOption(jobUUID,scheduleUUID,name,value,(String[])null);
  }

  /** set string schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, String value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%S",
                                              jobUUID,
                                              scheduleUUID,
                                              name,
                                              value
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set string schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setScheduleOption(String jobUUID, String scheduleUUID, String name, String value)
  {
    return setScheduleOption(jobUUID,scheduleUUID,name,value,(String[])null);
  }

  /** get server option value from BAR server
   * @param name name of value
   * @return value
   */
  public static <T> T getServerOption(String name, Class clazz)
  {
    T data = null;

    String[] errorMessage = new String[1];
    ValueMap resultMap    = new ValueMap();
    if (executeCommand(StringParser.format("SERVER_OPTION_GET name=%S",name),
                       0,
                       errorMessage,
                       resultMap
                      ) == Errors.NONE
       )
    {
      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Long.class)
      {
        data = (T)new Long(resultMap.getLong("value"));
      }
      else if (clazz == String.class)
      {
        data = (T)resultMap.get("value");
      }
    }

    return data;
  }

  /** get server option value from BAR server
   * @param widgetVariable widget variable
   * @return value
   */
  public static <T> T getServerOption(WidgetVariable widgetVariable, Class clazz)
  {
    return getServerOption(widgetVariable.getName(),clazz);
  }

  /** get boolean value from BAR server
   * @param name name of value
   * @return value
   */
  public static boolean getBooleanServerOption(String name)
  {
    return ((Boolean)getServerOption(name,Boolean.class)).booleanValue();
  }

  /** get long value from BAR server
   * @param name name of value
   * @return value
   */
  public static long getLongServerOption(String name)
  {
    return ((Long)getServerOption(name,Long.class)).longValue();
  }

  /** get string value from BAR server
   * @param name name of value
   * @return value
   */
  public static String getStringServerOption(String name)
  {
    return (String)getServerOption(name,String.class);
  }

  /** set boolean option value on BAR server
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, boolean value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                              name,
                                              value ? "yes" : "no"
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set boolean option value on BAR server
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, boolean value)
  {
    return setServerOption(name,value,(String[])null);
  }

  /** set long option value on BAR server
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, long value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%d",
                                              name,
                                              value
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set long job option value on BAR server
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, long value)
  {
    return setServerOption(name,value,(String[])null);
  }

  /** set string option value on BAR server
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, String value, String errorMessage[])
  {
    return executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%S",
                                              name,
                                              value
                                             ),
                          0,
                          errorMessage
                         );
  }

  /** set string option value on BAR server
   * @param name option name of value
   * @param value value
   * @return Errors.NONE or error code
   */
  public static int setServerOption(String name, String value)
  {
    return setServerOption(name,value,(String[])null);
  }

  /** get server option value on BAR server
   * @param widgetVariable widget variable
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int getServerOption(WidgetVariable widgetVariable, String errorMessage[])
  {
    ValueMap resultMap = new ValueMap();
    int error = executeCommand(StringParser.format("SERVER_OPTION_GET name=%S",widgetVariable.getName()),
                               0,
                               errorMessage,
                               resultMap
                              );
    if (error !=- Errors.NONE)
    {
      return error;
    }

    if      (widgetVariable.getType() == Boolean.class)
    {
      widgetVariable.set(resultMap.getBoolean("value"));
    }
    else if (widgetVariable.getType() == Long.class)
    {
      widgetVariable.set(resultMap.getLong("value"));
    }
    else if (widgetVariable.getType() == Double.class)
    {
      widgetVariable.set(resultMap.getDouble("value"));
    }
    else if (widgetVariable.getType() == String.class)
    {
      widgetVariable.set(resultMap.getString("value"));
    }
    else if (widgetVariable.getType() == Enum.class)
    {
//        widgetVariable.set(resultMap.getString("value"));
throw new Error("NYI");
    }
    else
    {
      throw new Error("Type not supported");
    }

    return error;
  }

  /** get server option value on BAR server
   * @param widgetVariable widget variable
   * @return Errors.NONE or error code
   */
  public static int getServerOption(WidgetVariable widgetVariable)
  {
    return getServerOption(widgetVariable,(String[])null);
  }

  /** set server option value on BAR server
   * @param widgetVariable widget variable
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int setServerOption(WidgetVariable widgetVariable, String errorMessage[])
  {
    int error = Errors.UNKNOWN;

    if      (widgetVariable.getType() == Boolean.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                                 widgetVariable.getName(),
                                                 widgetVariable.getBoolean() ? "yes" : "no"
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Long.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%d",
                                                 widgetVariable.getName(),
                                                 widgetVariable.getLong()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Double.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%f",
                                                 widgetVariable.getName(),
                                                 widgetVariable.getDouble()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == String.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%'S",
                                                 widgetVariable.getName(),
                                                 widgetVariable.getString()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Enum.class)
    {
/*
        error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                                   widgetVariable.getName(),
                                                   widgetVariable.getLong()
                                                  ),
                               0,
                               errorMessage
                              );
        break;*/
        throw new Error("NYI");
    }
    else
    {
      throw new Error("Type not supported");
    }

    return error;
  }

  /** set boolean option value on BAR server
   * @param widgetVariable widget variable
   * @return Errors.NONE or error code
   */
  public static int setServerOption(WidgetVariable widgetVariable)
  {
    return setServerOption(widgetVariable,(String[])null);
  }

  /** flush option value on BAR server
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int flushServerOption(String errorMessage[])
  {
    return executeCommand(StringParser.format("SERVER_OPTION_FLUSH"),
                          0,
                          errorMessage
                         );
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
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
      }
      catch (IllegalBlockSizeException exception)
      {
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
      }
      catch (BadPaddingException exception)
      {
        if (Settings.debugLevel > 0)
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

  public static ListDirectory remoteListDirectory = new ListDirectory()
  {
    /** remote file
     */
    class RemoteFile extends File
    {
      private FileTypes fileType;
      private long      size;
      private long      dateTime;

      /** create remote file
       * @param name name
       * @param fileType file type
       * @param size size [bytes]
       * @param dateTime last modified date/time
       */
      public RemoteFile(String name, FileTypes fileType, long size, long dateTime)
      {
        super(name);

        this.fileType = fileType;
        this.size     = size;
        this.dateTime = dateTime;
      }

      /** create remote file
       * @param name name
       * @param fileType file type
       * @param dateTime last modified date/time
       */
      public RemoteFile(String name, FileTypes fileType, long dateTime)
      {
        this(name,fileType,0,dateTime);
      }

      /** create remote file
       * @param name name
       * @param size size [bytes]
       */
      public RemoteFile(String name, long size)
      {
        this(name,FileTypes.DIRECTORY,size,0);
      }

      /** get file size
       * @return size [bytes]
       */
      public long length()
      {
        return size;
      }

      /** get last modified
       * @return last modified date/time
       */
      public long lastModified()
      {
        return dateTime*1000;
      }

      /** check if file is file
       * @return true iff file
       */
      public boolean isFile()
      {
        return fileType == FileTypes.FILE;
      }

      /** check if file is directory
       * @return true iff directory
       */
      public boolean isDirectory()
      {
        return fileType == FileTypes.DIRECTORY;
      }

      /** check if file is hidden
       * @return always false
       */
      public boolean isHidden()
      {
        return getName().startsWith(".");
      }

      /** check if file exists
       * @return always true
       */
      public boolean exists()
      {
        return true;
      }
    };

    private ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
    private Iterator<ValueMap>  iterator;

    /** get shortcut files
     * @return shortcut files
     */
    public File[] getShortcuts()
    {
      ArrayList<File> shortcutFileList = new ArrayList<File>();

      String[] errorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("ROOT_LIST"),
                                           0,
                                           errorMessage,
                                           valueMapList
                                          );
      if (error == Errors.NONE)
      {
        for (ValueMap valueMap : valueMapList)
        {
          shortcutFileList.add(new RemoteFile(valueMap.getString("name"),
                                              Long.parseLong(valueMap.getString("size"))
                                             )
                              );
        }
      }

      return shortcutFileList.toArray(new File[shortcutFileList.size()]);
    }

    /** set shortcut files
     * @param shortchuts shortcut files
     */
    public void setShortcuts(File shortcuts[])
    {
Dprintf.dprintf("");
    }

    /** open list files in directory
     * @param pathName path name
     * @return true iff open
     */
    public boolean open(String pathName)
    {
      String[] errorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("FILE_LIST directory=%'S",
                                                               pathName
                                                              ),
                                           0,
                                           errorMessage,
                                           valueMapList
                                          );
      if (error == Errors.NONE)
      {
        iterator = valueMapList.listIterator();
        return true;
      }
      else
      {
        return false;
      }
    }

    /** close list files in directory
     */
    public void close()
    {
      iterator = null;
    }

    /** get next entry in directory
     * @return entry
     */
    public File getNext()
    {
      File file = null;

      if (iterator.hasNext())
      {
        ValueMap valueMap = iterator.next();
        try
        {
          FileTypes fileType = valueMap.getEnum("fileType",FileTypes.class);
          switch (fileType)
          {
            case FILE:
              {
                String  name         = valueMap.getString ("name"         );
                long    size         = valueMap.getLong   ("size"         );
                long    dateTime     = valueMap.getLong   ("dateTime"     );
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.FILE,size,dateTime);
              }
              break;
            case DIRECTORY:
              {
                String  name         = valueMap.getString ("name"          );
                long    dateTime     = valueMap.getLong   ("dateTime"      );
                boolean noBackupFlag = valueMap.getBoolean("noBackup",false);
                boolean noDumpFlag   = valueMap.getBoolean("noDump",  false);

                file = new RemoteFile(name,FileTypes.DIRECTORY,dateTime);
              }
              break;
            case LINK:
              {
                String  name         = valueMap.getString ("name"    );
                long    dateTime     = valueMap.getLong   ("dateTime");
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.LINK,dateTime);
              }
              break;
            case HARDLINK:
              {
                String  name         = valueMap.getString ("name"    );
                long    size         = valueMap.getLong   ("size"    );
                long    dateTime     = valueMap.getLong   ("dateTime");
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.HARDLINK,size,dateTime);
              }
              break;
            case SPECIAL:
              {
                String  name         = valueMap.getString ("name"          );
                long    size         = valueMap.getLong   ("size",    0L   );
                long    dateTime     = valueMap.getLong   ("dateTime"      );
                boolean noBackupFlag = valueMap.getBoolean("noBackup",false);
                boolean noDumpFlag   = valueMap.getBoolean("noDump",  false);

                file = new RemoteFile(name,FileTypes.SPECIAL,dateTime);
              }
              break;
          }
        }
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugLevel > 0)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
        }
      }

      return file;
    }
  };

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
    StringBuilder stringBuffer = new StringBuilder(data.length*2);
    for (int z = 0; z < data.length; z++)
    {
      stringBuffer.append(String.format("%02x",(int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  /** start session: read session id, password encryption type and key
   * @param input,output input/output streams
   */
  private static void startSession(BufferedReader input, BufferedWriter output)
    throws IOException
  {
    sessionId           = null;
    passwordEncryptType = null;
    passwordCipher      = null;

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
    if (Settings.debugLevel > 1) System.err.println("Network: received '"+line+"'");
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
    if (sessionId == null)
    {
      throw new CommunicationError("No session id");
    }

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
        catch (Exception exception)
        {
          if (Settings.debugLevel > 0)
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
  }

  /** execute command syncronous
   * @param input,output input/output streams
   * @param commandString command string
   * @param typeMap types or null
   * @param errorMessage error message or ""
   * @param valueMap values or null
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(BufferedReader input, BufferedWriter output, String commandString, TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
    throws IOException
  {
    int errorCode;

    synchronized(output)
    {
      // new command
      commandId++;

      // send command
      String line = String.format("%d %s",commandId,commandString);
      output.write(line); output.write('\n'); output.flush();
      if (Settings.debugLevel > 1) System.err.println("Network: sent '"+line+"'");

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
        if (Settings.debugLevel > 1) System.err.println("Network: received '"+line+"'");

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
   * @param typeMap types or null
   * @param errorMessage error message or ""
   * @param valueMap values or null
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(String commandString, TypeMap typeMap, String[] errorMessage, ValueMap valueMap)
    throws IOException
  {
    return syncExecuteCommand(input,output,commandString,typeMap,errorMessage,valueMap);
  }

  /** execute command syncronous
   * @param input,output input/output streams
   * @param commandString command string
   * @param errorMessage error message or ""
   * @return Errors.NONE or error code
   */
  public static int syncExecuteCommand(BufferedReader input, BufferedWriter output, String commandString, String[] errorMessage)
    throws IOException
  {
    return syncExecuteCommand(input,output,commandString,(TypeMap)null,errorMessage,(ValueMap)null);
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
   * @param debugLevel debug level (0..n)
   * @param busyIndicator busy indicator or null
   * @param commandHandler command handler
   * @return Errors.NONE or error code
   */
  private static int executeCommand(String commandString, int debugLevel, BusyIndicator busyIndicator, CommandHandler commandHandler)
  {
    final int TIMEOUT = 120*1000; // total timeout [ms]

    Command command;
    int     errorCode = Errors.NONE;

    // send commadn
    synchronized(output)
    {
      if (busyIndicator != null)
      {
        busyIndicator.busy(0);
        if (busyIndicator.isAborted()) return Errors.ABORTED;
      }

      // add new command
      commandId++;
      command = readThread.commandAdd(commandId,TIMEOUT,debugLevel);

      // send command
      String line = String.format("%d %s",commandId,commandString);
      try
      {
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugLevel > debugLevel) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
        return Errors.NETWORK_SEND;
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

    // process results until completed or aborted
    int i = 0;
    do
    {
      while (   (errorCode == Errors.NONE)
             && ((busyIndicator == null) || !busyIndicator.isAborted())
             && !command.endOfData()
            )
      {
        errorCode = commandHandler.handleResult(i,command);
        if (busyIndicator != null)
        {
          busyIndicator.busy(0);
        }
        i++;
      }
    }
    while (   (errorCode == Errors.NONE)
           && ((busyIndicator == null) || !busyIndicator.isAborted())
           && !command.waitCompleted(250)
          );

    // free command
    readThread.commandRemove(command);

    // check if aborted
    if (busyIndicator != null)
    {
      if (busyIndicator.isAborted())
      {
        command.abort();
        errorCode = command.getErrorCode();
      }
    }

    return errorCode;
  }
}

/* end of file */
