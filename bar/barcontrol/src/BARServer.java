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
import java.io.FileInputStream;
import java.io.FileReader;
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

import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.security.interfaces.RSAPublicKey;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyFactory;
import java.security.KeyManagementException;
import java.security.KeyPair;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.security.Security;
import java.security.spec.RSAPublicKeySpec;
import java.security.UnrecoverableKeyException;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Random;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NullCipher;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManagerFactory;

import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter;
import org.bouncycastle.cert.X509CertificateHolder;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.jcajce.JcaPEMKeyConverter;
import org.bouncycastle.openssl.jcajce.JcePEMDecryptorProviderBuilder;
import org.bouncycastle.openssl.PEMDecryptorProvider;
import org.bouncycastle.openssl.PEMEncryptedKeyPair;
import org.bouncycastle.openssl.PEMKeyPair;
import org.bouncycastle.openssl.PEMParser;

import org.eclipse.swt.widgets.Display;

/****************************** Classes ********************************/

/** connection error
 */
class ConnectionError extends Error
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new connection error
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

/** BAR command
 */
class Command
{
  /** result handler
   */
  static abstract class ResultHandler
  {
    // --------------------------- constants --------------------------------

    // --------------------------- variables --------------------------------
    private boolean abortedFlag = false;

    // ------------------------ native functions ----------------------------

    // ---------------------------- methods ---------------------------------

    /** handle result
     * @param i result number 0..n
     * @param valueMap result
     */
    public int handle(int i, ValueMap valueMap)
    {
      return handle(valueMap);
    }

    /** handle result
     * @param valueMap result
     */
    public int handle(ValueMap valueMap)
    {
      throw new Error("result not handled");
    }

    /** check if aborted
     * @return true iff aborted
     */
    public boolean isAborted()
    {
      return abortedFlag;
    }

    /** abort command
     */
    public void abort()
    {
      abortedFlag = true;
    }
  }

  /** handler
   */
  static abstract class Handler
  {
    // --------------------------- constants --------------------------------

    // --------------------------- variables --------------------------------

    // ------------------------ native functions ----------------------------

    // ---------------------------- methods ---------------------------------

    abstract public int handle(Command command);
  }

  // --------------------------- constants --------------------------------
  public final static int TIMEOUT      = 30*1000;   // default timeout [ms]
  public final static int WAIT_FOREVER = -1;

  // --------------------------- variables --------------------------------

  private static long         commandId = 0;     // global command id counter

  public  final long          id;                // unique command id
  public  final String        string;            // command string
  public  int                 errorCode;         // error code
  public  String              errorMessage;      // error text
  public  boolean             completedFlag;     // true iff command completed
  public  boolean             abortedFlag;       // true iff command aborted
  public  int                 resultCount;       // result counter
  public  final ResultHandler resultHandler;     // result handler
  public  ArrayDeque<String>  result;            // result
  public  final Handler       handler;           // final handler
  public  final int           debugLevel;        // debug level

  private long                timeoutTimestamp;  // timeout timestamp [ms]

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create new command
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @param timeout timeout or 0 [ms]
   * @param resultHandler result handler
   * @param handler handler
   */
  Command(String commandString, int debugLevel, int timeout, ResultHandler resultHandler, Handler handler)
  {
    this.id               = getCommandId();
    this.string           = commandString;
    this.errorCode        = Errors.UNKNOWN;
    this.errorMessage     = "";
    this.completedFlag    = false;
    this.abortedFlag      = false;
    this.resultCount      = 0;
    this.resultHandler    = resultHandler;
    this.result           = new ArrayDeque<String>();
    this.handler          = handler;
    this.debugLevel       = debugLevel;
    this.timeoutTimestamp = (timeout != 0) ? System.currentTimeMillis()+timeout : 0L;
  }

  /** create new command
   * @param id command id
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @param timeout timeout or 0 [ms]
   */
  Command(String commandString, int debugLevel, int timeout)
  {
    this(commandString,debugLevel,timeout,(ResultHandler)null,(Handler)null);
  }

  /** create new command
   * @param id command id
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   */
  Command(String commandString, int debugLevel)
  {
    this(commandString,debugLevel,0);
  }

  /** send command
   */
  public void send(BufferedWriter output)
    throws IOException
  {
    String line = String.format("%d %s",id,string);
    output.write(line); output.write('\n'); output.flush();
    if (Settings.debugLevel > debugLevel) System.err.println("Network: sent '"+line+"'");
  }

  /** check if end of data
   * @return true iff command completed and all data processed
   */
  public synchronized boolean endOfData()
  {
    while (!completedFlag && !abortedFlag && (result.size() == 0))
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

  /** set completed
   */
  public synchronized void setCompleted()
  {
    completedFlag = true;
    notifyAll();
  }

  /** check if completed
   * @return true iff command aborted
   */
  public boolean isAborted()
  {
    return    abortedFlag
           || ((resultHandler != null) && resultHandler.isAborted());
  }

  /** set aborted
   */
  public synchronized void setAborted()
  {
    abortedFlag = true;
    notifyAll();
  }

  /** wait until command completed
   * @param timeout timeout [ms]
   * @return true if result available, false otherwise
   */
  public synchronized boolean waitForResult(long timeout)
  {
    boolean timeoutFlag = false;
    while (   !completedFlag
           && !abortedFlag
           && (result.size() == 0)
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
        timeoutFlag = true;
      }
    }

    return !timeoutFlag && (completedFlag || (result.size() > 0));
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
             && !abortedFlag
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
          timeoutFlag = true;
        }
      }
    }

    return !timeoutFlag && (completedFlag || abortedFlag);
  }

//TODO: remove
  /** wait until commmand completed
   */
  public void XwaitCompleted()
  {
    waitCompleted(WAIT_FOREVER);
  }

  /** set error
   * @param errorCode error code
   * @param errorMessage error text
   */
  public synchronized void setError(int errorCode, String errorMessage)
  {
    this.errorCode    = errorCode;
    this.errorMessage = errorMessage;
  }

  /** get error code
   * @return error code
   */
  public synchronized int getErrorCode()
  {
    return errorCode;
  }

  /** set error code
   * @param errorCode error code
   */
  public synchronized void setErrorCode(int errorCode)
  {
    this.errorCode = errorCode;
  }

  /** get error text
   * @return error text
   */
  public synchronized String getErrorMessage()
  {
    return errorMessage;
  }

  /** set error text
   * @param errorMessage error text
   */
  public synchronized void setErrorMessage(String errorMessage)
  {
    this.errorMessage = errorMessage;
  }

  /** get next resultg
   * @param timeout timeout [ms]
   * @return result string or null
   */
  public synchronized String getNextResult(long timeout)
  {
    while (!completedFlag && !abortedFlag && (result.size() == 0))
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
      result[0] = this.errorMessage;
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
      result.add(this.errorMessage);
    }

    return errorCode;
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
    // init variables
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
          if (!StringParser.parse(line,valueMap))
          {
            throw new RuntimeException("parse '"+line+"' fail");
          }
        }
      }
    }

    // get error message
    if (errorMessage != null) errorMessage[0] = this.errorMessage;

    return errorCode;
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

  /** get next result
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(errorMessage,valueMap);
  }

  /** get next result
   * @param typeMap type map
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public synchronized int getResult(ValueMap valueMap)
  {
    return getResult((String[])null,valueMap);
  }

  /** get result list
   * @param typeMap type map
   * @param errorMessage error message
   * @param valueMapList value map list
   * @return Errors.NONE or error code
   */
  public synchronized int getResults(String[] errorMessage, List<ValueMap> valueMapList)
  {
    if (errorCode == Errors.NONE)
    {
      while (!result.isEmpty())
      {
        String line = getNextResult();
        if (!line.isEmpty())
        {
          ValueMap valueMap = new ValueMap();
          StringParser.parse(line,valueMap);
          valueMapList.add(valueMap);
        }
      }
      if (errorMessage != null) errorMessage[0] = "";
    }
    else
    {
      if (errorMessage != null) errorMessage[0] = this.errorMessage;
    }

    return errorCode;
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
    if (!abortedFlag)
    {
      BARServer.abortCommand(this);
    }
  }

  /** timeout command
   */
  public void timeout()
  {
    if (!abortedFlag)
    {
      BARServer.timeoutCommand(this);
    }
  }

  /** convert data to string
   * @return string
   */
  public String toString()
  {
    return "Command {id="+id+", errorCode="+errorCode+", error="+errorMessage+", completedFlag="+completedFlag+", results="+result.size()+": "+string+"}";
  }

  private static synchronized long getCommandId()
  {
    commandId++;
    return commandId;
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
    String   line;
    ValueMap valueMap = new ValueMap();

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
          String  errorMessage  = "";
          String  data          = parts[3].trim();

          // store result
          Command command = commandHashMap.get(commandId);
          if (command != null)
          {
            synchronized(command)
            {
              if (Settings.debugLevel > command.debugLevel) System.err.println("Network: received '"+line+"'");

              if (completedFlag)
              {
                if (errorCode == Errors.NONE)
                {
                  if (command.resultHandler != null)
                  {
                    // parse and call result handler
                    if (!data.isEmpty())
                    {
                      valueMap.clear();
                      if (StringParser.parse(data,valueMap))
                      {
                        // call handler
                        errorCode    = command.resultHandler.handle(command.resultCount,valueMap);
                        errorMessage = "";
                      }
                      else
                      {
                        // parse error
                        errorCode    = Errors.NETWORK_PARSE;
                        errorMessage = "parse '"+data+"' fail";
                      }
                    }
                  }
                  else
                  {
                    // store result
                    errorCode = Errors.NONE;
                    if (!data.isEmpty())
                    {
                      command.result.add(data);
                      if (command.result.size() > 4096)
                      {
                        if (Settings.debugLevel > 0) System.err.println("Network: received "+command.result.size()+" results");
                      }
                      command.notifyAll();
                    }
                  }
                }
                else
                {
                  // error occurred
                  errorMessage = data;
                }

                // update command state
                command.setError(errorCode,errorMessage);
                command.setCompleted();

                // call command handler
                if (command.handler != null)
                {
                  command.handler.handle(command);
                }
              }
              else
              {
                if (command.resultHandler != null)
                {
                  // parse and call result handler
                  valueMap.clear();
                  if (StringParser.parse(data,valueMap))
                  {
                    // call handler
                    errorCode    = command.resultHandler.handle(command.resultCount,valueMap);
                    errorMessage = "";
                  }
                  else
                  {
                    // parse error
                    errorCode    = Errors.NETWORK_PARSE;
                    errorMessage = "parse '"+data+"' fail";
                  }
                }
                else
                {
                  // get result
                  errorCode = Errors.NONE;
                  if (!data.isEmpty())
                  {
                    command.result.add(data);
                    if (command.result.size() > 4096)
                    {
                      if (Settings.debugLevel > 0) System.err.println("Network: received "+command.result.size()+" results");
                    }
                    command.notifyAll();
                  }
                }

                // update command state
                command.setError(errorCode,errorMessage);
                if (errorCode != Errors.NONE)
                {
                  command.setCompleted();
                }
              }
              command.resultCount++;
            }
          }
          else
          {
            // result for unknown command -> currently ignored
            if (Settings.debugLevel > 0) System.err.println("Network: received unknown command result '"+line+"'");
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
        // communication impossible -> quit and cancel all commands with error
        synchronized(commandHashMap)
        {
          quitFlag = true;

          for (Command command : commandHashMap.values())
          {
            synchronized(command)
            {
              command.setError(Errors.NETWORK_RECEIVE,exception.getMessage());
              command.setCompleted();
              command.notifyAll();
              if (command.handler != null)
              {
                command.handler.handle(command);
              }
            }
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
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @param timeout timeout or 0 [ms]
   * @param resultHandler result handler
   * @param handler handler
   * @return command
   */
  public Command commandAdd(String commandString, int debugLevel, int timeout, Command.ResultHandler resultHandler, Command.Handler handler)
    throws ConnectionError
  {
    Command command = null;

    synchronized(commandHashMap)
    {
      if (!quitFlag)
      {
        command = new Command(commandString,debugLevel,timeout,resultHandler,handler);
        commandHashMap.put(command.id,command);
        if (commandHashMap.size() > 256)
        {
          if (Settings.debugLevel > 0) System.err.println("Network: warning "+commandHashMap.size()+" commands");
        }
        commandHashMap.notifyAll();
      }
      else
      {
        throw new ConnectionError("disconnected");
      }
    }

    return command;
  }

  /** add command
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @param timeout timeout or 0 [ms]
   * @return command
   */
  public Command commandAdd(String commandString, int debugLevel, int timeout)
    throws ConnectionError
  {
    return commandAdd(commandString,debugLevel,timeout,(Command.ResultHandler)null,(Command.Handler)null);
  }

  /** add command
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @return command
   */
  public Command commandAdd(String commandString, int debugLevel)
    throws ConnectionError
  {
    return commandAdd(commandString,debugLevel,0);
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

    return command.getErrorCode();
  }
}

/** BAR server
 */
public class BARServer
{
  // --------------------------- constants --------------------------------
  private final static int   PROTOCOL_VERSION_MAJOR = 4;
  private final static int   PROTOCOL_VERSION_MINOR = 0;

  public final static String DEFAULT_CA_FILE_NAME          = "bar-server-cert.pem";  // default certificate authority file name
  public final static String DEFAULT_CERTIFICATE_FILE_NAME = "bar-ca.pem";           // default certificate file name
  public final static String DEFAULT_KEY_FILE_NAME         = "bar-key.pem";          // default key file name
  public final static String DEFAULT_JAVA_KEY_FILE_NAME    = "bar.jks";              // default Java key file name

  public static char         fileSeparator;

  private final static int   SOCKET_READ_TIMEOUT    = 20*1000;                       // timeout reading socket [ms]
  private final static int   TIMEOUT                = 120*1000;                      // global timeout [ms]

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
  private static Object                      lock = new Object();
  private static JcaX509CertificateConverter certificateConverter;
  private static Display                     display = null;
  private static String                      name;
  private static int                         port;

  private static byte[]                      sessionId;
  private static String                      passwordEncryptType;
  private static Cipher                      passwordCipher;
  private static Key                         passwordKey;

  private static Socket                      socket;
  private static BufferedWriter              output;
  private static BufferedReader              input;
  private static ReadThread                  readThread;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  static
  {
    // add BouncyCastle as a security provider
    Security.addProvider(new BouncyCastleProvider());
    certificateConverter = new JcaX509CertificateConverter().setProvider("BC");

    // init random data
    Random random = new Random(System.currentTimeMillis());
    random.nextBytes(RANDOM_DATA);
  }

  /** connect to BAR server
   * @param display display
   * @param name host name
   * @param port host port number or 0
   * @param tlsPort TLS port number of 0
   * @param forceSSL TRUE to force SSL
   * @param password server password
   * @param caFileName server CA file name
   * @param certificateFileName server certificate file name
   * @param keyFileName server key file name
   */
  public static void connect(Display display,
                             String  name,
                             int     port,
                             int     tlsPort,
                             boolean forceSSL,
                             String  password,
                             String  caFileName,
                             String  certificateFileName,
                             String  keyFileName
                            )
  {
    /** key data
     */
    class KeyData
    {
      String caFileName;
      String certificateFileName;
      String keyFileName;
      String javaKeyFileName;

      KeyData(String caFileName,
              String certificateFileName,
              String keyFileName,
              String javaKeyFileName
             )
      {
        this.caFileName          = (caFileName          != null) ? caFileName          : DEFAULT_CA_FILE_NAME;
        this.certificateFileName = (certificateFileName != null) ? certificateFileName : DEFAULT_CERTIFICATE_FILE_NAME;
        this.keyFileName         = (keyFileName         != null) ? keyFileName         : DEFAULT_KEY_FILE_NAME;
        this.javaKeyFileName     = (javaKeyFileName     != null) ? javaKeyFileName     : DEFAULT_KEY_FILE_NAME;
      }
    };

    Socket         socket = null;
    BufferedWriter output = null;
    BufferedReader input  = null;

    assert name != null;
    assert (port != 0) || (tlsPort != 0);

    // get all possible certificate/key file names
    KeyData[] keyData_ = new KeyData[1+4];
    keyData_[0] = new KeyData(caFileName,
                              certificateFileName,
                              keyFileName,
                              keyFileName
                             );
    keyData_[1] = new KeyData(DEFAULT_CA_FILE_NAME,
                              DEFAULT_CERTIFICATE_FILE_NAME,
                              DEFAULT_KEY_FILE_NAME,
                              DEFAULT_JAVA_KEY_FILE_NAME
                             );
    keyData_[2] = new KeyData(System.getProperty("user.home")+File.separator+".bar"+File.separator+DEFAULT_CA_FILE_NAME,
                              System.getProperty("user.home")+File.separator+".bar"+File.separator+DEFAULT_CERTIFICATE_FILE_NAME,
                              System.getProperty("user.home")+File.separator+".bar"+File.separator+DEFAULT_KEY_FILE_NAME,
                              System.getProperty("user.home")+File.separator+".bar"+File.separator+DEFAULT_JAVA_KEY_FILE_NAME
                             );
    keyData_[3] = new KeyData(Config.CONFIG_DIR+File.separator+DEFAULT_CA_FILE_NAME,
                              Config.CONFIG_DIR+File.separator+DEFAULT_CERTIFICATE_FILE_NAME,
                              Config.CONFIG_DIR+File.separator+DEFAULT_KEY_FILE_NAME,
                              Config.CONFIG_DIR+File.separator+DEFAULT_JAVA_KEY_FILE_NAME
                             );
    keyData_[4] = new KeyData(Config.TLS_DIR+File.separator+"private"+File.separator+DEFAULT_CA_FILE_NAME,
                              Config.TLS_DIR+File.separator+"private"+File.separator+DEFAULT_CERTIFICATE_FILE_NAME,
                              Config.TLS_DIR+File.separator+"private"+File.separator+DEFAULT_KEY_FILE_NAME,
                              Config.TLS_DIR+File.separator+"private"+File.separator+DEFAULT_JAVA_KEY_FILE_NAME
                             );

    // connect to server: first try TLS, then plain
    String connectErrorMessage = null;
    if ((socket == null) && (port != 0))
    {
      // try to create TLS socket with PEM on plain socket+startSSL
      for (KeyData keyData : keyData_)
      {
        File caFile          = new File(keyData.caFileName);
        File certificateFile = new File(keyData.certificateFileName);
        File keyFile         = new File(keyData.keyFileName);
        if (   caFile.exists()          && caFile.isFile()          && caFile.canRead()
            && certificateFile.exists() && certificateFile.isFile() && certificateFile.canRead()
            && keyFile.exists()         && keyFile.isFile()         && keyFile.canRead()
           )
        {
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            sslSocketFactory = getSocketFactory(caFile,
                                                certificateFile,
                                                keyFile,
                                                ""
                                               );

            // create plain socket
            Socket plainSocket = new Socket(name,port);
            plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

            input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream()));

            // start session
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
Dprintf.dprintf("ssl info");
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
/**/

//java.security.cert.Certificate[] serverCerts = sslSocket.getSession().getPeerCertificates();
//Dprintf.dprintf("serverCerts=%s\n",serverCerts);

            // connection established => done
            socket = sslSocket;
            if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with PEM+startSSL (CA: "+caFile.getPath()+", Certificate: "+certificateFile.getPath()+", Key: "+keyFile.getPath()+")");
            break;
          }
          catch (ConnectionError exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
          catch (SocketTimeoutException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "unknown host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (IOException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
          }
          catch (Exception exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
        }
      }
    }

    if ((socket == null) && (tlsPort != 0))
    {
      // try to create TLS socket with PEM
      for (KeyData keyData : keyData_)
      {
        File caFile          = new File(keyData.caFileName);
        File certificateFile = new File(keyData.certificateFileName);
        File keyFile         = new File(keyData.keyFileName);
        if (   caFile.exists()          && caFile.isFile()          && caFile.canRead()
            && certificateFile.exists() && certificateFile.isFile() && certificateFile.canRead()
            && keyFile.exists()         && keyFile.isFile()         && keyFile.canRead()
           )
        {
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            sslSocketFactory = getSocketFactory(caFile,
                                                certificateFile,
                                                keyFile,
                                                ""
                                               );

            // create TLS socket
            sslSocket = (SSLSocket)sslSocketFactory.createSocket(name,tlsPort);
            sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
            sslSocket.startHandshake();

            input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream()));

            // start session
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
            if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with PEM ("+caFile.getPath()+")");
            break;
          }
          catch (SocketTimeoutException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "unknown host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (IOException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
          }
          catch (Exception exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
        }
      }
    }

    if ((socket == null) && (port != 0))
    {
      // try to create TLS socket with JKS on plain socket+startSSL
      for (KeyData keyData : keyData_)
      {
        File keyFile = new File(keyData.keyFileName);
        if (keyFile.exists() && keyFile.isFile() && keyFile.canRead())
        {
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            // check if valid Java key store
            KeyStore keystore = KeyStore.getInstance(KeyStore.getDefaultType());
            keystore.load(new FileInputStream(keyFile),null);

            // set Java key store to use
            System.setProperty("javax.net.ssl.trustStore",keyFile.getAbsolutePath());

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();

            // create plain socket
            Socket plainSocket = new Socket(name,port);
            plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

            input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream()));

            // start session
            startSession(input,output);

            // send startSSL on plain socket, wait for response
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
Dprintf.dprintf("ssl info");
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
/**/

//java.security.cert.Certificate[] serverCerts = sslSocket.getSession().getPeerCertificates();
//Dprintf.dprintf("serverCerts=%s\n",serverCerts);

            // connection established => done
            socket = sslSocket;
            if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with JKS+startSSL");
            break;
          }
          catch (ConnectionError exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
          catch (SocketTimeoutException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "unknown host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (IOException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
          }
          catch (Exception exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
        }
      }
    }

    if ((socket == null) && (tlsPort != 0))
    {
      // try to create TLS socket with JKS
      for (KeyData keyData : keyData_)
      {
        File keyFile = new File(keyData.keyFileName);
        if (keyFile.exists() && keyFile.isFile() && keyFile.canRead())
        {
          try
          {
            SSLSocketFactory sslSocketFactory;
            SSLSocket        sslSocket;

            // check if valid Java key store
            KeyStore keystore = KeyStore.getInstance(KeyStore.getDefaultType());
            keystore.load(new FileInputStream(keyFile),null);

            // set Java key store to use
            System.setProperty("javax.net.ssl.trustStore",keyFile.getAbsolutePath());

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();

            // create TLS socket
            sslSocket = (SSLSocket)sslSocketFactory.createSocket(name,tlsPort);
            sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
            sslSocket.startHandshake();

            input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
            output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream()));

            // start session
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
            if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with JKS");
            break;
          }
          catch (SocketTimeoutException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = "unknown host '"+name+((tlsPort != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
          }
          catch (IOException exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = BARControl.reniceIOException(exception).getMessage();
          }
          catch (Exception exception)
          {
            if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
          }
        }
      }
    }

    if ((socket == null) && (port != 0) && !forceSSL)
    {
      // try to create plain socket
      try
      {
        Socket plainSocket = new Socket(name,port);
        plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

        input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream()));
        output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream()));

        startSession(input,output);

        socket = plainSocket;
        if (Settings.debugLevel > 0) System.err.println("Network: plain socket");
      }
      catch (SocketTimeoutException exception)
      {
        if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
      }
      catch (ConnectException exception)
      {
        if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
      }
      catch (NoRouteToHostException exception)
      {
        if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' unreachable (no route to host)";
      }
      catch (UnknownHostException exception)
      {
        if (connectErrorMessage == null) connectErrorMessage = "unknown host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"'";
      }
      catch (Exception exception)
      {
        if (connectErrorMessage == null) connectErrorMessage = exception.getMessage();
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
                                                 encryptPassword(password)
                                                ),
                             errorMessage
                            ) != Errors.NONE
         )
      {
        throw new ConnectionError("Authorization fail");
      }

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
    synchronized(lock)
    {
      if (BARServer.socket != null)
      {
        disconnect();
      }

      // setup new connection
      BARServer.display = display;
      BARServer.name    = name;
      BARServer.port    = socket.getPort();
      BARServer.socket  = socket;
      BARServer.input   = input;
      BARServer.output  = output;
    }

    // start read thread
    readThread = new ReadThread(input);
    readThread.start();
  }

  /** connect to BAR server
   * @param name host name
   * @param port host port number or 0
   * @param tlsPort TLS port number of 0
   * @param forceSSL TRUE to force SSL
   * @param password server password
   * @param caFileName server CA file name
   * @param certificateFileName server certificate file name
   * @param keyFileName server key file name
   */
  public static void connect(String  name,
                             int     port,
                             int     tlsPort,
                             boolean forceSSL,
                             String  password,
                             String  caFileName,
                             String  certificateFileName,
                             String  keyFileName
                            )
  {
    connect((Display)null,
            name,
            port,
            tlsPort,
            forceSSL,
            password,
            caFileName,
            certificateFileName,
            keyFileName
           );
  }

  /** disconnect from BAR server
   */
  public static void disconnect()
  {
    try
    {
      // flush data (ignore errors)
      executeCommand("JOB_FLUSH",0);
    }
    catch (CommunicationError error)
    {
      // ignored
    }
    catch (ConnectionError error)
    {
      // ignored
    }
    catch (Throwable throwable)
    {
      if (Settings.debugLevel > 0)
      {
        System.err.println("ERROR: "+throwable.getMessage());
        BARControl.printStackTrace(throwable);
        System.exit(1);
      }
    }

    synchronized(lock)
    {
      if (readThread == null)
      {
        return;
      }

      try
      {
        // close connection, stop read thread
        readThread.quit();
        BARServer.socket.close(); BARServer.socket = null;
        try { readThread.join(); } catch (InterruptedException exception) { /* ignored */ }; readThread = null;

        // free resources
        BARServer.input.close(); BARServer.input = null;
        BARServer.output.close(); BARServer.output = null;
      }
      catch (IOException exception)
      {
        // ignored
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("ERROR: "+throwable.getMessage());
          BARControl.printStackTrace(throwable);
          System.exit(1);
        }
      }
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

  /** get server name
   * @return server name
   */
  public static String getName()
  {
    return name;
  }

  /** get server port
   * @return server port
   */
  public static int getPort()
  {
    return port;
  }

  /** get server info
   * @return server info
   */
  public static String getInfo()
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append(name);
    buffer.append(':');
    buffer.append(port);
    if (socket instanceof SSLSocket)
    {
      buffer.append(" (TLS)");
    }

    return buffer.toString();
  }

  /** start running command
   * @param commandString command to start
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   * @return command
   */
  public static Command runCommand(String commandString, int debugLevel, Command.ResultHandler resultHandler, Command.Handler handler)
  {
    Command command = null;
    synchronized(lock)
    {
      if (readThread == null)
      {
        return null;
      }

      try
      {
        // add new command
        command = readThread.commandAdd(commandString,debugLevel,TIMEOUT,resultHandler,handler);

        // send command
        String line = String.format("%d %s",command.id,commandString);
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugLevel > command.debugLevel) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        if (command != null) readThread.commandRemove(command);
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
    return runCommand(commandString,debugLevel,(Command.ResultHandler)null,(Command.Handler)null);
  }

  /** abort command execution
   * @param command command to abort
   * @return Errors.NONE or error code
   */
  static void abortCommand(Command command)
  {
    // send abort for command
    executeCommand(StringParser.format("ABORT commandId=%d",command.id),0);
    removeCommand(command);

    // set error aborted
    command.setError(Errors.ABORTED,"aborted");
    command.result.clear();
    command.setAborted();
  }

  /** timeout command execution
   * @param command command to abort
   * @return Errors.NONE or error code
   */
  static void timeoutCommand(Command command)
  {
    // send abort for command
    executeCommand(StringParser.format("ABORT commandId=%d",command.id),0);
    removeCommand(command);

    // set error timeout
    command.setError(Errors.NETWORK_TIMEOUT,"timeout");
    command.result.clear();
    command.setAborted();
  }

  /** asyncronous execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @param busyIndicator busy indicator or null
   * @return command
   */
  public static Command asyncExecuteCommand(String                commandString,
                                            int                   debugLevel,
                                            Command.ResultHandler resultHandler,
                                            Command.Handler       handler,
                                            BusyIndicator         busyIndicator
                                           )
  {
    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        return null;
      }
    }

    // create and send command
    Command command = null;
    synchronized(lock)
    {
      if (readThread == null)
      {
        return null;
      }

      try
      {
        // add new command
        command = readThread.commandAdd(commandString,debugLevel,TIMEOUT,resultHandler,handler);

        // send command
        String line = String.format("%d %s",command.id,command.string);
        output.write(line); output.write('\n'); output.flush();
        if (Settings.debugLevel > debugLevel) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        if (command != null) readThread.commandRemove(command);
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
        return null;
      }
    }

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        abortCommand(command);
        return null;
      }
    }

    return command;
  }

  /** asyncronous execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @return command
   */
  public static Command asyncExecuteCommand(String                commandString,
                                            int                   debugLevel,
                                            Command.ResultHandler resultHandler,
                                            Command.Handler       handler
                                           )
  {
    return asyncExecuteCommand(commandString,debugLevel,resultHandler,handler,(BusyIndicator)null);
  }

  /** asyncronous execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   * @return command
   */
  public static Command asyncExecuteCommand(String                commandString,
                                            int                   debugLevel,
                                            Command.ResultHandler resultHandler
                                           )
  {
    return asyncExecuteCommand(commandString,debugLevel,resultHandler,(Command.Handler)null);
  }

  /** asyncronous execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @return command
   */
  public static Command asyncExecuteCommand(String commandString,
                                            int    debugLevel
                                           )
  {
    return asyncExecuteCommand(commandString,debugLevel,(Command.ResultHandler)null);
  }

  /** wait for asynchronous command
   * @param command command to send to BAR server
   * @param errorMessage error message or null
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int asyncCommandWait(Command       command,
                                     String[]      errorMessage,
                                     BusyIndicator busyIndicator
                                    )
  {
    final int SLEEP_TIME = 250;  // [ms]

    long t0;

    // process results until error, completed, or aborted
    if ((display != null) && (Thread.currentThread() == display.getThread()))
    {
      display.update();
    }
    while (   ((busyIndicator == null) || !busyIndicator.isAborted())
           && !command.isCompleted()
           && !command.isAborted()
          )
    {
      if ((display != null) && (Thread.currentThread() == display.getThread()))
      {
        // if this is the GUI thread run GUI loop
        final boolean done[] = new boolean[]{ false };
        display.timerExec(250,new Runnable() { public void run() { done[0] = true; display.wake(); } });
        while (   !done[0]
               && !display.isDisposed()
               && !display.readAndDispatch())
        {
          display.sleep();
        }
      }
      else
      {
        // wait for command completion
        command.waitCompleted(SLEEP_TIME);
      }

      if (busyIndicator != null)
      {
        busyIndicator.busy(0);
      }
    }
    if (command.isAborted())
    {
      removeCommand(command);
      return Errors.ABORTED;
    }

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        abortCommand(command);
        return Errors.ABORTED;
      }
    }

    if (errorMessage != null) errorMessage[0] = command.getErrorMessage();

    removeCommand(command);

    return command.getErrorCode();
  }

  /** wait for asynchronous command
   * @param command command to send to BAR server
   * @param errorMessage error message or null
   * @return Errors.NONE or error code
   */
  public static int asyncCommandWait(Command  command,
                                     String[] errorMessage
                                    )
  {
    return asyncCommandWait(command,errorMessage,(BusyIndicator)null);
  }

  /** wait for asynchronous command
   * @param command command to send to BAR server
   * @return Errors.NONE or error code
   */
  public static int asyncCommandWait(Command command)
  {
    return asyncCommandWait(command,(String[])null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                commandString,
                                   int                   debugLevel,
                                   final String[]        errorMessage,
                                   Command.ResultHandler resultHandler,
                                   Command.Handler       handler,
                                   BusyIndicator         busyIndicator
                                  )
  {
    if (errorMessage != null) errorMessage[0] = null;

    // create and send command
    Command command = asyncExecuteCommand(commandString,
                                          debugLevel,
                                          resultHandler,
                                          handler,
                                          busyIndicator
                                         );

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        abortCommand(command);
        return Errors.ABORTED;
      }
    }

    // process results until error, completed, or aborted
    int errorCode = asyncCommandWait(command,
                                     errorMessage,
                                     busyIndicator
                                    );

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        return Errors.ABORTED;
      }
    }

    return errorCode;
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                commandString,
                                   int                   debugLevel,
                                   final String[]        errorMessage,
                                   Command.ResultHandler resultHandler,
                                   Command.Handler       handler
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,resultHandler,handler,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   * @param handler handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                commandString,
                                   int                   debugLevel,
                                   Command.ResultHandler resultHandler,
                                   Command.Handler       handler
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null,resultHandler,handler);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                commandString,
                                   int                   debugLevel,
                                   final String[]        errorMessage,
                                   Command.ResultHandler resultHandler,
                                   BusyIndicator         busyIndicator
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,resultHandler,(Command.Handler)null,busyIndicator);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                commandString,
                                   int                   debugLevel,
                                   final String[]        errorMessage,
                                   Command.ResultHandler resultHandler
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,resultHandler,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String                 commandString,
                                   int                   debugLevel,
                                   Command.ResultHandler resultHandler
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null,resultHandler);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param handler handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String          commandString,
                                   int             debugLevel,
                                   final String[]  errorMessage,
                                   Command.Handler handler
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,(Command.ResultHandler)null,handler);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param handler handler
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String          commandString,
                                   int             debugLevel,
                                   Command.Handler handler
                                  )
  {
    return executeCommand(commandString,debugLevel,(String[])null,handler);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param valueMap value map
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String         commandString,
                                   int            debugLevel,
                                   final String[] errorMessage,
                                   final ValueMap valueMap,
                                   BusyIndicator  busyIndicator
                                  )
  {
    if (valueMap != null) valueMap.clear();

    return executeCommand(commandString,
                          debugLevel,
                          errorMessage,
                          null,  // result handler
                          new Command.Handler()
                          {
                            public int handle(Command command)
                            {
                              return command.getResult(errorMessage,valueMap);
                            }
                          },
                          busyIndicator
                         );
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param valueMap value map
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String   commandString,
                                   int      debugLevel,
                                   String[] errorMessage,
                                   ValueMap valueMap
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message ornull
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
   * @param errorMessage error message or null
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
   * @param errorMessage error message or null
   * @param valueMapList value map list
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String               commandString,
                                   int                  debugLevel,
                                   final String[]       errorMessage,
                                   final List<ValueMap> valueMapList,
                                   BusyIndicator        busyIndicator)
  {
    if (errorMessage != null) errorMessage[0] = null;
    if (valueMapList != null) valueMapList.clear();

    return executeCommand(commandString,
                          debugLevel,
                          errorMessage,
                          null,  // resultHandler
                          new Command.Handler()
                          {
                            public int handle(Command command)
                            {
                              return command.getResults(errorMessage,valueMapList);
                            }
                          },
                          busyIndicator
                         );
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param valueMapList value map list
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String         commandString,
                                   int            debugLevel,
                                   String[]       errorMessage,
                                   List<ValueMap> valueMapList
                                  )
  {
    return executeCommand(commandString,debugLevel,errorMessage,valueMapList,(BusyIndicator)null);
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
      assert resultMap.size() > 0;

      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Integer.class)
      {
        data = (T)new Integer(resultMap.getInt("value"));
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
    if (error != Errors.NONE)
    {
      return error;
    }
    assert resultMap.size() > 0;

    if      (widgetVariable.getType() == Boolean.class)
    {
      widgetVariable.set(resultMap.getBoolean("value"));
    }
    else if (widgetVariable.getType() == Integer.class)
    {
      widgetVariable.set(resultMap.getInt("value"));
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
    else if (widgetVariable.getType() == Integer.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%d",
                                                 jobUUID,
                                                 widgetVariable.getName(),
                                                 widgetVariable.getInteger()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Long.class)
    {
      error = executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%ld",
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
      assert resultMap.size() > 0;

      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Integer.class)
      {
        data = (T)new Integer(resultMap.getInt("value"));
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
      assert resultMap.size() > 0;

      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value"));
      }
      else if (clazz == Integer.class)
      {
        data = (T)new Integer(resultMap.getInt("value"));
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
    else if (widgetVariable.getType() == Integer.class)
    {
      widgetVariable.set(resultMap.getInt("value"));
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
    else if (widgetVariable.getType() == Integer.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%d",
                                                 widgetVariable.getName(),
                                                 widgetVariable.getInteger()
                                                ),
                             0,
                             errorMessage
                            );
    }
    else if (widgetVariable.getType() == Long.class)
    {
      error = executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%ld",
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

  /** remote file
   */
  static class RemoteFile extends File
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
     * @param fileType file type
     */
    public RemoteFile(String name, FileTypes fileType)
    {
      this(name,fileType,0);
    }

    /** create remote file
     * @param name name
     * @param size size [bytes]
     */
    public RemoteFile(String name, long size)
    {
      this(name,FileTypes.DIRECTORY,size,0);
    }

    /** create remote file
     * @param name name
     */
/*    public RemoteFile(String name)
    {
      this(name,0);
    }*/

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

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "RemoteFile {"+getPath()+", "+fileType+", "+size+","+dateTime+"}";
    }
  };

  /** list remote directory
   */
  public static ListDirectory<RemoteFile> remoteListDirectory = new ListDirectory<RemoteFile>()
  {
    private ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
    private Iterator<ValueMap>  iterator;

    public RemoteFile newInstance(String name)
    {
      FileTypes fileType = FileTypes.FILE;
      long      size     = 0;
      long      dateTime = 0;

      ValueMap valueMap = new ValueMap();
      int error = BARServer.executeCommand(StringParser.format("FILE_INFO name=%'S",
                                                               name
                                                              ),
                                           1,  // debugLevel
                                           null,  // errorMessage,
                                           valueMap
                                          );
      if (error == Errors.NONE)
      {
        fileType = valueMap.getEnum("fileType",FileTypes.class);
        switch (fileType)
        {
          case FILE:
            size = valueMap.getLong("size");
          case HARDLINK:
            break;
          case DIRECTORY:
          case LINK:
          case SPECIAL:
            break;
          default:
            break;
        }
        dateTime = valueMap.getLong("dateTime");
      }

      return new RemoteFile(name,fileType,size,dateTime);
    }

    /** get shortcut files
     * @return shortcut files
     */
    public void getShortcuts(ArrayList<RemoteFile> shortcutList)
    {
      final HashMap<String,RemoteFile> shortcutMap = new HashMap<String,RemoteFile>();

      // add manual shortcuts
      for (String name : Settings.shortcuts)
      {
        shortcutMap.put(name,new RemoteFile(name,FileTypes.DIRECTORY));
      }

      // add root shortcuts
      BARServer.executeCommand(StringParser.format("ROOT_LIST"),
                               1,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 public int handle(int i, ValueMap valueMap)
                                 {
                                   String name = valueMap.getString("name");
                                   long   size = Long.parseLong(valueMap.getString("size"));

                                   shortcutMap.put(name,new RemoteFile(name,size));

                                   return Errors.NONE;
                                 }
                               }
                              );

      shortcutList.clear();
      for (RemoteFile shortcut : shortcutMap.values())
      {
        shortcutList.add(shortcut);
      }
      Collections.sort(shortcutList,this);
    }

    /** remove shortcut file
     * @param name shortcut name
     */
    public void addShortcut(RemoteFile shortcut)
    {
      Settings.shortcuts.add(shortcut.getAbsolutePath());
    }

    /** remove shortcut file
     * @param shortcut shortcut file
     */
    public void removeShortcut(RemoteFile shortcut)
    {
      Settings.shortcuts.remove(shortcut.getAbsolutePath());
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
                                           1,  // debugLevel
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

  /** create SSL socket factor from with PEM files
   * original from: https://gist.github.com/rohanag12/07ab7eb22556244e9698
   * @param certificateAuthorityFile certificate authority PEM file
   * @param certificateFile certificate PEM file
   * @param keyFile PEM key file
   * @param password password or null
   * @return socket factory
   */
  public static SSLSocketFactory getSocketFactory(File   certificateAuthorityFile,
                                                  File   certificateFile,
                                                  File   keyFile,
                                                  String password
                                                 )
    throws KeyManagementException,NoSuchAlgorithmException,KeyStoreException,UnrecoverableKeyException,IOException,CertificateException
  {
    char[]                passwordChars;
    PEMParser             reader;
    X509CertificateHolder certificateHolder;
    KeyStore              keyStore;

    passwordChars = (password != null) ? password.toCharArray() : new char[0];

    // load certificate authority (CA) certificate
    reader = new PEMParser(new FileReader(certificateAuthorityFile));
    certificateHolder = (X509CertificateHolder)reader.readObject();
    reader.close();
    X509Certificate certificateAuthority = certificateConverter.getCertificate(certificateHolder);

    // load certificate
    reader = new PEMParser(new FileReader(certificateFile));
    certificateHolder = (X509CertificateHolder)reader.readObject();
    reader.close();
    X509Certificate certificate = certificateConverter.getCertificate(certificateHolder);

    // load private key
    reader = new PEMParser(new FileReader(keyFile));
    Object keyObject = reader.readObject();
    reader.close();
    PEMDecryptorProvider pemDecryptorProvider = new JcePEMDecryptorProviderBuilder().build(passwordChars);
    JcaPEMKeyConverter keyConverter = new JcaPEMKeyConverter().setProvider("BC");

    // init key pair
    KeyPair key;
    if (keyObject instanceof PEMEncryptedKeyPair)
    {
      key = keyConverter.getKeyPair(((PEMEncryptedKeyPair)keyObject).decryptKeyPair(pemDecryptorProvider));
    }
    else
    {
      key = keyConverter.getKeyPair((PEMKeyPair)keyObject);
    }

    // CA certificate used to authenticate server
    keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
    keyStore.load(null,null);
    keyStore.setCertificateEntry("ca-certificate",certificateAuthority);
    TrustManagerFactory trustManagerFactory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
    trustManagerFactory.init(keyStore);

    // certificate and client key for authentication
    keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
    keyStore.load(null,null);
    keyStore.setCertificateEntry("certificate",certificate);
    keyStore.setKeyEntry("private-key",
                         key.getPrivate(),
                         passwordChars,
                         new Certificate[]{certificate}
                        );
    KeyManagerFactory keyManagerFactory = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
    keyManagerFactory.init(keyStore,passwordChars);

    // create SSL socket factory
    SSLContext sslContext = SSLContext.getInstance("TLSv1.2");
    sslContext.init(keyManagerFactory.getKeyManagers(),
                    trustManagerFactory.getTrustManagers(),
                    null
                   );

    return sslContext.getSocketFactory();
  }

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

    synchronized(lock)
    {
      // get new command
      Command command = new Command(commandString,0);

      // send command
      String line = String.format("%d %s",command.id,command.string);
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
      while (Integer.parseInt(data[0]) != command.id);

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

  /** remove command
   * @param command command
   */
  public static void removeCommand(Command command)
  {
    synchronized(lock)
    {
      if (readThread != null)
      {
        readThread.commandRemove(command);
      }
    }
  }
}

/* end of file */
