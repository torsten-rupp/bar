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

import java.text.SimpleDateFormat;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
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

import org.eclipse.swt.SWTException;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;

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

  /** create new communication error
   * @param exception BAR exception
   */
  CommunicationError(BARException exception)
  {
    super(exception);
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
  /** command result handler
   */
  static abstract class ResultHandler
  {
    // --------------------------- constants --------------------------------

    // --------------------------- variables --------------------------------
    private boolean abortedFlag = false;

    // ------------------------ native functions ----------------------------

    // ---------------------------- methods ---------------------------------

    /** handle result
     * Note: called for every result received from the server
     * @param i result number 0..n
     * @param valueMap result
     */
    public void handle(int i, ValueMap valueMap)
      throws BARException
    {
      handle(valueMap);
    }

    /** handle result
     * Note: called for every result received from the server
     * @param valueMap result
     * @return ERROR_NONE or error code
     */
    public void handle(ValueMap valueMap)
      throws BARException
    {
      throw new Error("result not handled: %s");
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

  /** command handler
   */
  static abstract class Handler
  {
    // --------------------------- constants --------------------------------

    // --------------------------- variables --------------------------------

    // ------------------------ native functions ----------------------------

    // ---------------------------- methods ---------------------------------

    /** handle result
     * Note: called once for final result received from the server
     * @param command command
     */
    public void handle(Command command)
    {
      handle(command.errorCode,command.errorData,command.valueMap);
    }

    /** handle result
     * Note: called once for final result received from the server
     * @param errorCode BARException.NONE or error code
     * @param errorData "" or error data (iff error != Erros.NONE)
     * @param valueMap value map (iff error != ERROR_NONE)
     */
    public void handle(int errorCode, String errorData, ValueMap valueMap)
    {
      throw new Error("result not handled");
    }
  }

  // --------------------------- constants --------------------------------
  public final static int TIMEOUT      = 30*1000;   // default timeout [ms]
  public final static int WAIT_FOREVER = -1;

  /** actions
   */
  enum Actions
  {
    NONE,
    REQUEST_PASSWORD,
    REQUEST_VOLUME,
    CONFIRM;
  };

  /** password types
   */
  enum PasswordTypes
  {
    NONE,
    FTP,
    SSH,
    WEBDAV,
    CRYPT;

    /** check if login password
     * @return true iff login password
     */
    public boolean isLogin()
    {
      return (this == FTP) || (this == SSH) || (this == WEBDAV);
    }

    /** check if crypt password
     * @return true iff crypt password
     */
    public boolean isCrypt()
    {
      return (this == CRYPT);
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case NONE:   return "none";
        case FTP:    return "FTP";
        case SSH:    return "SSH";
        case WEBDAV: return "WebDAV";
        case CRYPT:  return "encryption";
      }

      return "";
    }
  };

  // --------------------------- variables --------------------------------

  private static long         commandId = 0;     // global command id counter

  public  final long          id;                // unique command id
  public  final String        string;            // command string
  public  ValueMap            valueMap;
  public  int                 resultCount;       // result counter
  public  final ResultHandler resultHandler;     // result handler
  public  ArrayDeque<String>  resultList;        // result list
  public  final Handler       handler;           // final handler
  public  final int           debugLevel;        // debug level

  private int                 errorCode;         // error code
  private String              errorData;         // error data
  private boolean             completedFlag;     // true iff command completed
  private boolean             abortedFlag;       // true iff command aborted
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
    this.errorCode        = BARException.UNKNOWN;
    this.errorData        = "";
    this.valueMap         = new ValueMap();
    this.completedFlag    = false;
    this.abortedFlag      = false;
    this.resultCount      = 0;
    this.resultHandler    = resultHandler;
    this.resultList       = new ArrayDeque<String>();
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
    BARServer.logSent(debugLevel,"%s",line);
  }

  /** check if end of data
   * @return true iff command completed and all data processed
   */
  public synchronized boolean endOfData()
  {
    while (!completedFlag && !abortedFlag && resultList.isEmpty())
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

    return completedFlag && resultList.isEmpty();
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

  /** check if aborted
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
           && resultList.isEmpty()
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

    return !timeoutFlag && (completedFlag || !resultList.isEmpty());
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

  /** set error
   * @param errorCode error code
   * @param errorData error data
   */
  public synchronized void setError(int errorCode, String errorData)
  {
    this.errorCode = errorCode;
    this.errorData = errorData;
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

  /** get error data
   * @return error data
   */
  public synchronized String getErrorData()
  {
    return errorData;
  }

  /** set error data
   * @param errorData error data
   */
  public synchronized void setErrorData(String errorData)
  {
    this.errorData = errorData;
  }

  /** get next resultg
   * @param timeout timeout [ms]
   * @return result string or null
   */
  public synchronized String getNextResult(long timeout)
  {
    while (!completedFlag && !abortedFlag && resultList.isEmpty())
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

    return resultList.pollFirst();
  }

  /** get next result
   * @return result string or null
   */
  public synchronized String getNextResult()
  {
    return resultList.pollFirst();
  }

  /** get result string array
   * @param result result string array to fill
   * @return BARException.NONE or error code
   */
  public synchronized int getResult(String result[])
  {
    if (errorCode == BARException.NONE)
    {
      result[0] = !this.resultList.isEmpty() ? this.getNextResult() : "";
    }
    else
    {
      result[0] = this.errorData;
    }

    return errorCode;
  }

  /** get result string list array
   * @param resultList result string list
   * @return BARException.NONE or error code
   */
  public synchronized int getResults(List<String> resultList)
  {
    if (errorCode == BARException.NONE)
    {
      resultList.clear();
      resultList.addAll(this.resultList);
      this.resultList.clear();
    }
    else
    {
      resultList.add(this.errorData);
    }

    return errorCode;
  }

  /** get result list
   * @param errorMessage error message
   * @param valueMapList value map list
   * @return BARException.NONE or error code
   */
//TODO: remove
  public synchronized int getResults(String[] errorMessage, List<ValueMap> valueMapList)
  {
    if (errorCode == BARException.NONE)
    {
      while (!resultList.isEmpty())
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
      if (errorMessage != null) errorMessage[0] = this.errorData;
    }

    return errorCode;
  }
  /** get result list
   * @param valueMapList value map list
   * @return BARException.NONE or error code
   */
  public synchronized int getResults2(List<ValueMap> valueMapList)
  {
    if (errorCode == BARException.NONE)
    {
      while (!resultList.isEmpty())
      {
        String line = getNextResult();
        if (!line.isEmpty())
        {
          ValueMap valueMap = new ValueMap();
          StringParser.parse(line,valueMap);
          valueMapList.add(valueMap);
        }
      }
    }

    return errorCode;
  }

  /** get next result
   * @param errorMessage error message
   * @param valueMap value map
   * @param timeout timeout or WAIT_FOREVER [ms]
   * @return BARException.NONE or error code
   */
  public synchronized int getNextResult(String[] errorMessage, ValueMap valueMap, int timeout)
  {
    if (errorCode == BARException.NONE)
    {
      // wait for result
      waitForResult(timeout);

      // parse next line
      if (!resultList.isEmpty())
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
    if (errorMessage != null) errorMessage[0] = this.errorData;

    return errorCode;
  }

  /** get next result
   * @param errorMessage error message
   * @param valueMap value map
   * @return BARException.NONE or error code
   */
  public synchronized int getNextResult(String[] errorMessage, ValueMap valueMap)
  {
    return getNextResult(errorMessage,valueMap,0);
  }

  /** get final result
   * @param errorMessage error message or null
   * @param valueMap value map or null
   * @return BARException.NONE or error code
   */
  public synchronized int getResult(String[] errorMessage, ValueMap valueMap)
  {
    if (errorMessage != null)
    {
      errorMessage[0] = this.errorData;
    }
    if (valueMap != null)
    {
      valueMap.clear();
      for (String name : this.valueMap.keySet())
      {
        valueMap.put(name,this.valueMap.get(name));
      }
    }

    return this.errorCode;
  }

  /** get final result
   * @param valueMap value map or null
   * @return BARException.NONE or error code
   */
  public synchronized int getResult(ValueMap valueMap)
  {
    return getResult((String[])null,valueMap);
  }

  /** purge all results
   */
  public synchronized void purgeResults()
  {
    String lastResult = resultList.pollLast();
    resultList.clear();
    if (lastResult != null) resultList.add(lastResult);
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
    return "Command {id="+id+", errorCode="+errorCode+", errorData="+errorData+", completedFlag="+completedFlag+", results="+resultList.size()+": "+string+"}";
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
  private Display               display;
  private BufferedReader        input;
  private boolean               quitFlag = false;
  private HashMap<Long,Command> commandHashMap = new HashMap<Long,Command>();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create read thread
   * @param input input stream
   */
  ReadThread(Display display, BufferedReader input)
  {
    this.display = display;
    this.input   = input;
    setDaemon(true);
    setName("BARControl Server Read");
  }

  /** run method
   */
  public void run()
  {
    final int RESULT_WARNING = 4096;

    String line;
    Object arguments[] = new Object[4];

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

          if      (StringParser.parse(line,"%lu %y %u % S",arguments))
          {
            // result: <id> <completed flag> <error code> <data>

            // get command id, completed flag, error code, data
            long    commandId     = (Long)arguments[0];
            boolean completedFlag = (Boolean)arguments[1];
            int     errorCode     = (Integer)arguments[2];
            String  data          = ((String)arguments[3]).trim();

            // handle/store result
            Command command = commandHashMap.get(commandId);
            if (command != null)
            {
              synchronized(command)
              {
                BARServer.logReceived(command.debugLevel,"%s",line);

                if (errorCode == BARException.NONE)
                {
                  // parse result
                  command.valueMap.clear();
                  if (!data.isEmpty())
                  {
                    if (StringParser.parse(data,command.valueMap))
                    {
                      if (command.resultHandler != null)
                      {
                        // call handler for every result
                        try
                        {
                          command.resultHandler.handle(command.resultCount,(ValueMap)command.valueMap.clone());
                        }
                        catch (IllegalArgumentException exception)
                        {
                          throw exception;
                        }
                        catch (Exception exception)
                        {
                          throw new BARException(BARException.PROCESS,exception.getMessage());
                        }
                      }
                      else
                      {
                        // store result
                        command.resultList.add(data);
                        if (command.resultList.size() > RESULT_WARNING)
                        {
                          BARServer.logReceived(command.debugLevel,"%d %s",command.resultList.size(),line);
                        }
                        command.notifyAll();
                      }
                      command.resultCount++;
                    }
                    else
                    {
                      // parse error
                      throw new BARException(BARException.PARSE,data);
                    }
                  }

                  // update command error info+state
                  command.setErrorCode(BARException.NONE);
                  if (completedFlag)
                  {
                    command.setCompleted();

                    if (command.handler != null)
                    {
                      // call handler for final result
                      try
                      {
                        command.handler.handle(command);
                      }
                      catch (Throwable throwable)
                      {
                        // ignored
                      }
                    }

                    command.notifyAll();
                  }
                }
                else
                {
                  // error occurred
                  command.setError(errorCode,data);
                  command.setCompleted();
                  command.notifyAll();
                }
              }
            }
            else
            {
              // result for unknown command -> currently ignored
              BARServer.logReceived(1,"unknown command result %s",line);
            }
          }
          else if (StringParser.parse(line,"%lu %S % S",arguments))
          {
            // command: <id> <name> <data>

            // get command id, name, data
            long    commandId = (Long)arguments[0];
            String  name      = ((String)arguments[1]);
            String  data          = ((String)arguments[2]).trim();

            // parse data
            ValueMap valueMap = new ValueMap();
            if (!StringParser.parse(data,valueMap))
            {
              // parse error
              throw new BARException(BARException.PARSE,data);
            }

            // process command
            BARServer.process(commandId,name,valueMap);
          }
          else
          {
            throw new CommunicationError("malformed command or result '"+line+"'");
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
              command.setError(BARException.NETWORK_RECEIVE,exception.getMessage());
              command.setCompleted();

              // call final handler
              if (command.handler != null)
              {
                try
                {
                  command.handler.handle(command);
                }
                catch (Throwable throwable)
                {
                  // ignored
                }
              }

              command.notifyAll();
            }
          }
        }
      }
      catch (final SWTException exception)
      {
        System.err.println("INTERNAL ERROR: "+exception.getCause());
        BARControl.printStackTrace(exception);
        System.err.println("Version "+BARControl.VERSION);
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.showFatalError(exception);
          }
        });
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
      }
      catch (final AssertionError error)
      {
        System.err.println("INTERNAL ERROR: "+error.getMessage());
        BARControl.printStackTrace(error);
        System.err.println("Version "+BARControl.VERSION);
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.showFatalError(error);
          }
        });
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
      }
      catch (final InternalError error)
      {
        System.err.println("INTERNAL ERROR: "+error.getMessage());
        BARControl.printStackTrace(error);
        System.err.println("Version "+BARControl.VERSION);
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.showFatalError(error);
          }
        });
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
      }
      catch (final Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("INTERNAL ERROR: "+throwable.getMessage());
          BARControl.printStackTrace(throwable);
          System.err.println("Version "+BARControl.VERSION);
        }
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.showFatalError(throwable);
          }
        });
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
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
          if (Settings.debugLevel > 0) System.err.println(String.format("Network warning %8d: %d commands",commandHashMap.size()));
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

/** server command thread
 */
class CommandThread extends Thread
{
  /** command
   */
  class Command
  {
    long     id;
    String   name;
    ValueMap valueMap;

    /** create command
     * @param id command id
     * @param name command name
     * @param valueMap command value map
     */
    Command(long id, String name, ValueMap valueMap)
    {
      this.id       = id;
      this.name     = name;
      this.valueMap = valueMap;
    }
  }

  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private Display               display;
  private BufferedWriter        output;
  private boolean               quitFlag = false;
  private ArrayDeque<Command>   commandQueue = new ArrayDeque<Command>();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create command thread
   * @param output output stream
   */
  CommandThread(Display display, BufferedWriter output)
  {
    this.display = display;
    this.output  = output;
    setDaemon(true);
    setName("BARControl Server Command");
  }

  /** run method
   */
  public void run()
  {
    Command command;

    while (!quitFlag)
    {
      // wait for command or quit
      synchronized(commandQueue)
      {
        do
        {
          command = commandQueue.pollFirst();
          if (command == null)
          try
          {
            commandQueue.wait();
          }
          catch (InterruptedException exception)
          {
            // ignored
          }
        }
        while (!quitFlag && (command == null));
      }
      if (quitFlag) break;

      // process
      if      (command.name.equals("CONFIRM"))
      {
        String type = command.valueMap.getString("type");

        if      (type.equals("RESTORE"))
        {
          // get confirm data
          final String storageName = command.valueMap.getString("storageName","");
          final String entryName   = command.valueMap.getString("entryName","");
          final String errorData   = command.valueMap.getString("errorData","");

          // confirm dialog
          final String  action[]      = new String[]{"ABORT"};
          final boolean skipAllFlag[] = new boolean[]{false};
          display.syncExec(new Runnable()
          {
            public void run()
            {
              switch (Dialogs.select(new Shell(),
                                     BARControl.tr("Confirmation"),
                                     BARControl.tr("Cannot restore:\n\n {0}\n\nReason: {1}",
                                                   !entryName.isEmpty() ? entryName : storageName,
                                                   errorData
                                                  ),
                                     new String[]{BARControl.tr("Skip"),BARControl.tr("Skip all"),BARControl.tr("Abort")},
                                     0
                                    )
                     )
              {
                case 0:
                  action[0]      = "SKIP";
                  skipAllFlag[0] = false;
                  break;
                case 1:
                  action[0]      = "SKIP";
                  skipAllFlag[0] = true;
                  break;
                case 2:
                  action[0]      = "ABORT";
                  skipAllFlag[0] = false;
                  break;
              }
            }
          });

          // send result
          try
          {
            BARServer.sendResult(command.id,1,true,0,"action=%s skipAll=%s",action[0],skipAllFlag[0] ? "yes" : "no");
          }
          catch (BARException exception)
          {
            // ignored
          }
        }
      }
      else if (command.name.equals("REQUEST_PASSWORD"))
      {
//        if      (type.equals("LOGIN"))
//        {
          final String        name         = command.valueMap.getString("name","");
          final PasswordTypes passwordType = command.valueMap.getEnum  ("passwordType",PasswordTypes.class,PasswordTypes.NONE);
          final String        passwordText = command.valueMap.getString("passwordText","");
//          final String        volume       = command.valueMap.getString("volume","");
//          final int           errorCode    = command.valueMap.getInt   ("errorCode",BARException.NONE);
//          final String        errorData    = command.valueMap.getString("errorData","");
//          final String        storageName  = command.valueMap.getString("storageName","");
//          final String        entryName    = command.valueMap.getString("entryName","");

          // get password
          display.syncExec(new Runnable()
          {
            @Override
            public void run()
            {
              if (passwordType.isLogin())
              {
                String[] data = Dialogs.login(new Shell(),
                                              BARControl.tr("{0} login password",passwordType),
                                              BARControl.tr("Please enter {0} login for: {1}",passwordType,passwordText),
                                              name,
                                              BARControl.tr("Password")+":"
                                             );
                if (data != null)
                {
                  BARServer.asyncExecuteCommand(StringParser.format("ACTION_RESULT errorCode=%d name=%S encryptType=%s encryptedPassword=%S",
                                                                    BARException.NONE,
                                                                    data[0],
                                                                    BARServer.getPasswordEncryptType(),
                                                                    BARServer.encryptPassword(data[1])
                                                                   ),
                                                0  // debugLevel
                                               );
                }
                else
                {
                  BARServer.asyncExecuteCommand(StringParser.format("ACTION_RESULT errorCode=%d",
                                                                    BARException.NO_PASSWORD
                                                                   ),
                                                0  // debugLevel
                                               );
                }
              }
              else
              {
                String password = Dialogs.password(new Shell(),
                                                   BARControl.tr("{0} login password",passwordType),
                                                   BARControl.tr("Please enter {0} password for: {1}",passwordType,passwordText),
                                                   BARControl.tr("Password")+":"
                                                  );
                if (password != null)
                {
                  BARServer.asyncExecuteCommand(StringParser.format("ACTION_RESULT errorCode=%d encryptType=%s encryptedPassword=%S",
                                                                    BARException.NONE,
                                                                    BARServer.getPasswordEncryptType(),
                                                                    BARServer.encryptPassword(password)
                                                                   ),
                                                0  // debugLevel
                                               );
                }
                else
                {
                  BARServer.asyncExecuteCommand(StringParser.format("ACTION_RESULT errorCode=%d",
                                                                    BARException.NO_PASSWORD
                                                                   ),
                                                0  // debugLevel
                                               );
                }
              }
            }
          });
//        }
      }
      else if (command.name.equals("REQUEST_VOLUME"))
      {
//TODO
Dprintf.dprintf("REQUEST_VOLUME");
System.exit(1);
      }
      else
      {
Dprintf.dprintf("unknown command %s",command.name);
Dprintf.halt();
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

  /** process command
   * @param id command id
   * @param name command name
   * @param valueMap command value map
   */
  public void process(long id, String name, ValueMap valueMap)
  {
    synchronized(commandQueue)
    {
      commandQueue.add(new Command(id,name,valueMap));
      commandQueue.notifyAll();
    }
  }
}

/** BAR server
 */
public class BARServer
{
  // --------------------------- constants --------------------------------
  private final static int              PROTOCOL_VERSION_MAJOR = 5;
  private final static int              PROTOCOL_VERSION_MINOR = 0;

  public final static String            DEFAULT_CA_FILE_NAME          = "bar-ca.pem";           // default certificate authority file name
  public final static String            DEFAULT_CERTIFICATE_FILE_NAME = "bar-server-cert.pem";  // default certificate file name
  public final static String            DEFAULT_KEY_FILE_NAME         = "bar-key.pem";          // default key file name
  public final static String            DEFAULT_JAVA_KEY_FILE_NAME    = "bar.jks";              // default Java key file name

  public static String                  masterName;                                             // master name or null
  public static char                    fileSeparator;

  private final static int              SOCKET_READ_TIMEOUT    = 30*1000;                       // timeout reading socket [ms]
  private final static int              TIMEOUT                = 120*1000;                      // global timeout [ms]

  private static final SimpleDateFormat LOG_TIME_FORMAT = new SimpleDateFormat("HH:mm:ss:SSS");
  private static Date                   logTime0        = new Date();

  private static byte[]                 RANDOM_DATA = new byte[64];

  /** modes
   */
  enum Modes
  {
    MASTER,
    SLAVE
  };

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
  private static Modes                       mode;

  private static Socket                      socket;
//  private static BufferedWriter              output;
public static BufferedWriter              output;
  private static BufferedReader              input;
  private static ReadThread                  readThread;
  private static CommandThread               commandThread;

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
        if (   (keyData.caFileName          != null)
            && (keyData.certificateFileName != null)
            && (keyData.keyFileName         != null)
           )
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
              plainSocket.setTcpNoDelay(true);

              input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream(),"UTF-8"));

              // accept session
              acceptSession(input,output);

              // send startSSL, wait for response
              String[] errorMessage = new String[1];
              syncExecuteCommand(input,
                                 output,
                                 StringParser.format("START_SSL"),
                                 2  // debugLevel
                                );

              // create TLS socket on plain socket
              sslSocket = (SSLSocket)sslSocketFactory.createSocket(plainSocket,name,tlsPort,false);
              sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
              sslSocket.setTcpNoDelay(true);
              sslSocket.startHandshake();

              input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream(),"UTF-8"));

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
              if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with PEM key+startSSL (CA: "+caFile.getPath()+", Certificate: "+certificateFile.getPath()+", Key: "+keyFile.getPath()+")");
              break;
            }
            catch (BARException exception)
            {
              if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' (error: "+exception.getMessage()+")";
            }
            catch (ConnectionError error)
            {
              if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' (error: "+error.getMessage()+")";
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
    }

    if ((socket == null) && (tlsPort != 0))
    {
      // try to create TLS socket with PEM
      for (KeyData keyData : keyData_)
      {
        if (   (keyData.caFileName          != null)
            && (keyData.certificateFileName != null)
            && (keyData.keyFileName         != null)
           )
        {
          File caFile          = new File(keyData.caFileName);
          File certificateFile = new File(keyData.certificateFileName);
          File keyFile         = new File(keyData.keyFileName);
          if (   (caFile          != null) && caFile.exists()          && caFile.isFile()          && caFile.canRead()
              && (certificateFile != null) && certificateFile.exists() && certificateFile.isFile() && certificateFile.canRead()
              && (keyFile         != null) && keyFile.exists()         && keyFile.isFile()         && keyFile.canRead()
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

              input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream(),"UTF-8"));

              // accept session
              acceptSession(input,output);

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
              if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with PEM key ("+caFile.getPath()+")");
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
    }

    if ((socket == null) && (port != 0))
    {
      // try to create TLS socket with JKS on plain socket+startSSL
      for (KeyData keyData : keyData_)
      {
        if (keyData.keyFileName != null)
        {
          File keyFile = new File(keyData.keyFileName);
          if ((keyFile != null) && keyFile.exists() && keyFile.isFile() && keyFile.canRead())
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

              input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream(),"UTF-8"));

              // accept session
              acceptSession(input,output);

              // send startSSL on plain socket, wait for response
              String[] errorMessage = new String[1];
              syncExecuteCommand(input,
                                 output,
                                 StringParser.format("START_SSL"),
                                 2  // debugLevel
                                );

              // create TLS socket on plain socket
              sslSocket = (SSLSocket)sslSocketFactory.createSocket(plainSocket,name,tlsPort,false);
              sslSocket.setSoTimeout(SOCKET_READ_TIMEOUT);
              sslSocket.startHandshake();

              input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream(),"UTF-8"));

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
              if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with JKS key+startSSL");
              break;
            }
            catch (BARException exception)
            {
              if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' (error: "+exception.getMessage()+")";
            }
            catch (ConnectionError error)
            {
              if (connectErrorMessage == null) connectErrorMessage = "host '"+name+((port != Settings.DEFAULT_SERVER_PORT) ? ":"+port : "")+"' (error: "+error.getMessage()+")";
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
    }

    if ((socket == null) && (tlsPort != 0))
    {
      // try to create TLS socket with JKS
      for (KeyData keyData : keyData_)
      {
        if (keyData.keyFileName != null)
        {
          File keyFile = new File(keyData.keyFileName);
          if ((keyFile != null) && keyFile.exists() && keyFile.isFile() && keyFile.canRead())
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

              input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream(),"UTF-8"));
              output = new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream(),"UTF-8"));

              // accept session
              acceptSession(input,output);

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
              if (Settings.debugLevel > 0) System.err.println("Network: TLS socket with JKS key");
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
    }

    if ((socket == null) && (port != 0) && !forceSSL)
    {
      try
      {
        // create plain socket
        Socket plainSocket = new Socket(name,port);
        plainSocket.setSoTimeout(SOCKET_READ_TIMEOUT);

        input  = new BufferedReader(new InputStreamReader(plainSocket.getInputStream(),"UTF-8"));
        output = new BufferedWriter(new OutputStreamWriter(plainSocket.getOutputStream(),"UTF-8"));

        // accept session
        acceptSession(input,output);

        socket = plainSocket;
        if (Settings.debugLevel > 0) System.err.println("Network: plain socket");
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
        connectErrorMessage = exception.getMessage();
      }
    }

    if (socket == null)
    {
      if   ((tlsPort != 0) || (port!= 0)) throw new ConnectionError(connectErrorMessage);
      else                                throw new ConnectionError("no server ports specified");
    }

    // authorize, get version/file separator
    try
    {
      String[] errorMessage = new String[1];
      ValueMap valueMap     = new ValueMap();

      // authorize with password
      syncExecuteCommand(input,
                         output,
                         StringParser.format("AUTHORIZE encryptType=%s encryptedPassword=%s",
                                             passwordEncryptType,
                                             encryptPassword(password)
                                            ),
                         2  // debugLevel
                        );

      // get version, mode
      syncExecuteCommand(input,
                         output,
                         "VERSION",
                         2,  // debugLevel
                         valueMap
                        );
//TODO        throw new ConnectionError("Cannot get protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': "+errorMessage[0]);
      if ((valueMap.getInt("major") != PROTOCOL_VERSION_MAJOR) && !Settings.debugIgnoreProtocolVersion)
      {
        throw new CommunicationError("Incompatible protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': expected "+PROTOCOL_VERSION_MAJOR+", got "+valueMap.getInt("major"));
      }
      if (valueMap.getInt("minor") != PROTOCOL_VERSION_MINOR)
      {
        BARControl.printWarning("Incompatible minor protocol version for '"+name+((socket.getPort() != Settings.DEFAULT_SERVER_PORT) ? ":"+socket.getPort() : "")+"': expected "+PROTOCOL_VERSION_MINOR+", got "+valueMap.getInt("minor"));
      }
      mode = valueMap.getEnum("mode",Modes.class,Modes.MASTER);

      // get master name
      syncExecuteCommand(input,
                         output,
                         "MASTER_GET",
                         2,  // debugLevel
                         valueMap
                        );
      masterName = valueMap.getString("name",null);

      // get file separator character
      syncExecuteCommand(input,
                         output,
                         "GET name=FILE_SEPARATOR",
                         2,  // debugLevel
                         valueMap
                        );
      fileSeparator = valueMap.getString("value","/").charAt(0);
    }
    catch (BARException exception)
    {
      throw new CommunicationError("Network error on "+socket.getInetAddress()+":"+socket.getPort()+": "+exception.getMessage());
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error on "+socket.getInetAddress()+":"+socket.getPort()+": "+exception.getMessage());
    }

    synchronized(lock)
    {
      // disconnect if previously connected
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

    // start read thread, command thread
    readThread = new ReadThread(display,input);
    readThread.start();
    commandThread = new CommandThread(display,output);
    commandThread.start();
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
      // flush data (ignore BARException)
      executeCommand("JOB_FLUSH",0);
    }
    catch (BARException exception)
    {
      // ignored
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
        System.err.println("INTERNAL ERROR: "+throwable.getMessage());
        BARControl.printStackTrace(throwable);
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
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
          System.err.println("INTERNAL ERROR: "+throwable.getMessage());
          BARControl.printStackTrace(throwable);
          System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
        }
      }
    }
  }

  /** check if master-mode
   * @return true iff master-mode
   */
  public static boolean isMaster()
  {
    return mode == Modes.MASTER;
  }

  /** check if slave-mode
   * @return true iff slave-mode
   */
  public static boolean isSlave()
  {
    return mode == Modes.SLAVE;
  }

  /** get master name
   * @return master name
   */
  public static String getMasterName()
  {
    return masterName;
  }

  /** quit BAR server (for debug only)
   * @return true if quit command sent, false otherwise
   */
  public static boolean quit()
  {
    try
    {
      // flush data
      try
      {
        executeCommand("JOB_FLUSH",0);
      }
      catch (BARException exception)
      {
        // ignored
      }

      // send QUIT command
      try
      {
        executeCommand("QUIT",0);
      }
      catch (BARException exception)
      {
        return false;
      }

      // sleep a short time
      try { Thread.sleep(1000); } catch (InterruptedException exception) { /* ignored */ }

      // close connection, stop read thread
      commandThread.quit();
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
        logSent(debugLevel,"%s",line);
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
   * @return BARException.NONE or error code
   */
  static void abortCommand(Command command)
  {
    // send abort for command
    try
    {
      executeCommand(StringParser.format("ABORT commandId=%d",command.id),1);
    }
    catch (BARException exception)
    {
      // ignored
    }
    removeCommand(command);

    // set error aborted
    command.setError(BARException.ABORTED,"aborted");
    command.resultList.clear();
    command.setAborted();
  }

  /** timeout command execution
   * @param command command to abort
   * @return BARException.NONE or error code
   */
  static void timeoutCommand(Command command)
  {
    // send abort for command
    try
    {
      executeCommand(StringParser.format("ABORT commandId=%d",command.id),1);
    }
    catch (BARException exception)
    {
      // ignored
    }
    removeCommand(command);

    // set error timeout
    command.setError(BARException.NETWORK_TIMEOUT,"timeout");
    command.resultList.clear();
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
        logSent(debugLevel,"%s",line);
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
   * @return BARException.NONE or error code
   */
  public static void asyncCommandWait(Command       command,
                                      BusyIndicator busyIndicator
                                     )
    throws BARException
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
      BARServer.abortCommand(command);
    }

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        abortCommand(command);
      }
    }

    // remove command from receive thread
    removeCommand(command);

    // create error
    if (command.getErrorCode() != BARException.NONE)
    {
      throw new BARException(command.getErrorCode(),command.getErrorData());
    }
  }

  /** wait for asynchronous command
   * @param command command to send to BAR server
   */
  public static void asyncCommandWait(Command command)
    throws BARException
  {
    asyncCommandWait(command,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @param busyIndicator busy indicator or null
   * @return BARException.NONE or error code
   */
  public static void executeCommand(String                commandString,
                                    int                   debugLevel,
                                    Command.ResultHandler resultHandler,
                                    Command.Handler       handler,
                                    BusyIndicator         busyIndicator
                                   )
    throws BARException
  {
    // create and send command
    Command command = asyncExecuteCommand(commandString,
                                          debugLevel,
                                          resultHandler,
                                          handler,
                                          busyIndicator
                                         );
    if (command == null)
    {
      throw new BARException(BARException.ABORTED);
    }

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        abortCommand(command);
        throw new BARException(BARException.ABORTED);
      }
    }

    // wait and process results until error, completed, or aborted
    asyncCommandWait(command,busyIndicator);

    // update busy indicator, check if aborted
    if (busyIndicator != null)
    {
      busyIndicator.busy(0);
      if (busyIndicator.isAborted())
      {
        throw new BARException(BARException.ABORTED);
      }
    }
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   * @param handler handler
   */
  public static void executeCommand(String                commandString,
                                    int                   debugLevel,
                                    Command.ResultHandler resultHandler,
                                    Command.Handler       handler
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,resultHandler,handler,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   * @param busyIndicator busy indicator or null
   */
  public static void executeCommand(String                commandString,
                                    int                   debugLevel,
                                    Command.ResultHandler resultHandler,
                                    BusyIndicator         busyIndicator
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,resultHandler,(Command.Handler)null,busyIndicator);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param resultHandler result handler
   */
  public static void executeCommand(String                commandString,
                                    int                   debugLevel,
                                    Command.ResultHandler resultHandler
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,resultHandler,(BusyIndicator)null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param handler handler
   */
  public static void executeCommand(String          commandString,
                                    int             debugLevel,
                                    Command.Handler handler
                                   )
  {
    executeCommand(commandString,debugLevel,handler);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param valueMapList value map list
   * @param busyIndicator busy indicator or null
   */
  public static void executeCommand(String               commandString,
                                    int                  debugLevel,
                                    final List<ValueMap> valueMapList,
                                    BusyIndicator        busyIndicator
                                   )
    throws BARException
  {
    if (valueMapList != null) valueMapList.clear();

    executeCommand(commandString,
                   debugLevel,
                   null,  // resultHandler
                   new Command.Handler()
                   {
                     public void handle(Command command)
                     {
                       command.getResults2(valueMapList);
                     }
                   },
                   busyIndicator
                  );
  }

  /** execute command
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param valueMapList value map list
   */
  public static void executeCommand(String         commandString,
                                    int            debugLevel,
                                    List<ValueMap> valueMapList
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,valueMapList,(BusyIndicator)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param valueMap value map
   * @param busyIndicator busy indicator or null
   */
  public static void executeCommand(String         commandString,
                                    int            debugLevel,
                                    final ValueMap valueMap,
                                    BusyIndicator  busyIndicator
                                   )
    throws BARException
  {
    if (valueMap != null) valueMap.clear();

    executeCommand(commandString,
                   debugLevel,
                   null,  // result handler
                   new Command.Handler()
                   {
                     public void handle(Command command)
                     {
                       command.getResult(valueMap);
                     }
                   },
                   busyIndicator
                  );
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param valueMap value map
   */
  public static void executeCommand(String   commandString,
                                    int      debugLevel,
                                    ValueMap valueMap
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,valueMap,(BusyIndicator)null);
  }

  /** execute command
   * @param commandString command to send to BAR server
   * @param debugLevel debug level (0..n)
   */
  public static void executeCommand(String commandString,
                                    int    debugLevel
                                   )
    throws BARException
  {
    executeCommand(commandString,debugLevel,(ValueMap)null);
  }

  /** send result
   * @param command command to send to BAR server
   * @param debugLevel debug level (0..n)
   * @param errorMessage error message or null
   * @param resultHandler result handler
   * @param handler handler
   * @param busyIndicator busy indicator or null
   * @return BARException.NONE or error code
   */
  public static void sendResult(long      commandId,
                                int       debugLevel,
                                boolean   completedFlag,
                                int       error,
                                String    format,
                                Object... arguments
                               )
    throws BARException
  {
    synchronized(lock)
    {
      if (readThread == null)
      {
        return;
      }

      try
      {
        // format result
        String data = String.format(format,arguments);

        // send result
        String line = String.format("%d %d %d %s\n",commandId,completedFlag ? 1 : 0,error,data);
        output.write(line); output.flush();
        logSent(debugLevel,"%s",line);
      }
      catch (IOException exception)
      {
        throw new CommunicationError(exception.getMessage());
      }
    }
  }

  // ----------------------------------------------------------------------

  /** set boolean value on BAR server
   * @param name name of value
   * @param value value
   */
  public static void set(String name, boolean value)
    throws BARException
  {
    executeCommand(StringParser.format("SET name=%s value=%s",name,value ? "yes" : "no"),0);
  }

  /** set long value on BAR server
   * @param name name of value
   * @param value value
   */
  static void set(String name, long value)
    throws BARException
  {
    executeCommand(StringParser.format("SET name=%s value=%d",name,value),0);
  }

  /** set string value on BAR server
   * @param name name of value
   * @param value value
   */
  public static void set(String name, String value)
    throws BARException
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

    try
    {
      ValueMap resultMap = new ValueMap();
      executeCommand(StringParser.format("JOB_OPTION_GET jobUUID=%s name=%S",jobUUID,name),
                     0,
                     resultMap
                    );
      assert resultMap.size() > 0;

      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(resultMap.getBoolean("value",false));
      }
      else if (clazz == Integer.class)
      {
        data = (T)new Integer(resultMap.getInt("value",0));
      }
      else if (clazz == Long.class)
      {
        data = (T)new Long(resultMap.getLong("value",0L));
      }
      else if (clazz == String.class)
      {
        data = (T)resultMap.getString("value","");
      }
    }
    catch (BARException exception)
    {
      if      (clazz == Boolean.class)
      {
        data = (T)new Boolean(false);
      }
      else if (clazz == Integer.class)
      {
        data = (T)new Integer(0);
      }
      else if (clazz == Long.class)
      {
        data = (T)new Long(0L);
      }
      else if (clazz == String.class)
      {
        data = (T)new String("");
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
   */
  public static void setJobOption(String jobUUID, String name, boolean value)
  {
    try
    {
      executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                         jobUUID,
                                         name,
                                         value ? "yes" : "no"
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set long job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   */
  public static void setJobOption(String jobUUID, String name, long value)
  {
    try
    {
      executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%d",
                                         jobUUID,
                                         name,
                                         value
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set string job option value on BAR server
   * @param jobUUID job UUID
   * @param name option name of value
   * @param value value
   */
  public static void setJobOption(String jobUUID, String name, String value)
  {
    try
    {
      executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%S",
                                         jobUUID,
                                         name,
                                         value
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** get string value from BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   */
  public static void getJobOption(String jobUUID, WidgetVariable widgetVariable)
  {
    try
    {
      ValueMap resultMap = new ValueMap();
      executeCommand(StringParser.format("JOB_OPTION_GET jobUUID=%s name=%S",jobUUID,widgetVariable.getName()),
                     0,
                     resultMap
                    );
      assert resultMap.size() > 0;

      if      (widgetVariable.getType() == Boolean.class)
      {
        widgetVariable.set(resultMap.getBoolean("value",false));
      }
      else if (widgetVariable.getType() == Integer.class)
      {
        widgetVariable.set(resultMap.getInt("value",0));
      }
      else if (widgetVariable.getType() == Long.class)
      {
        widgetVariable.set(resultMap.getLong("value",0L));
      }
      else if (widgetVariable.getType() == Double.class)
      {
        widgetVariable.set(resultMap.getDouble("value",0.0));
      }
      else if (widgetVariable.getType() == String.class)
      {
        widgetVariable.set(resultMap.getString("value",""));
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
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set job option value on BAR server
   * @param jobUUID job UUID
   * @param widgetVariable widget variable
   */
  public static void setJobOption(String jobUUID, WidgetVariable widgetVariable)
  {
    try
    {
      if      (widgetVariable.getType() == Boolean.class)
      {
        executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                           jobUUID,
                                           widgetVariable.getName(),
                                           widgetVariable.getBoolean() ? "yes" : "no"
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Integer.class)
      {
        executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%d",
                                           jobUUID,
                                           widgetVariable.getName(),
                                           widgetVariable.getInteger()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Long.class)
      {
        executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%ld",
                                           jobUUID,
                                           widgetVariable.getName(),
                                           widgetVariable.getLong()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Double.class)
      {
        executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%f",
                                           jobUUID,
                                           widgetVariable.getName(),
                                           widgetVariable.getDouble()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == String.class)
      {
        executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%'S",
                                           jobUUID,
                                           widgetVariable.getName(),
                                           widgetVariable.getString()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Enum.class)
      {
  /*
          executeCommand(StringParser.format("JOB_OPTION_SET jobUUID=%s name=%S value=%s",
                                             jobUUID,
                                             widgetVariable.getName(),
                                             widgetVariable.getLong()
                                            ),
                       0  // debugLevel
                        );
          break;*/
          throw new Error("NYI");
      }
      else
      {
        throw new Error("Type not supported");
      }
    }
    catch (BARException exception)
    {
      // ignored
    }
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

    try
    {
      ValueMap resultMap = new ValueMap();
      executeCommand(StringParser.format("SCHEDULE_OPTION_GET jobUUID=%s scheduleUUID=%s name=%S",jobUUID,scheduleUUID,name),
                     0,  // debugLevel
                     resultMap
                    );
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
    catch (BARException exception)
    {
      // ignored
    }

    return data;
  }

  /** set boolean schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   * @param errorMessage error message or ""
   */
  public static void setScheduleOption(String jobUUID, String scheduleUUID, String name, boolean value)
  {
    try
    {
      executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%s",
                                         jobUUID,
                                         scheduleUUID,
                                         name,
                                         value ? "yes" : "no"
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set long schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   */
  public static void setScheduleOption(String jobUUID, String scheduleUUID, String name, long value)
  {
    try
    {
      executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%d",
                                         jobUUID,
                                         scheduleUUID,
                                         name,
                                         value
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set string schedule option value on BAR server
   * @param jobUUID job UUID
   * @param scheduleUUID schedule UUID
   * @param name option name of value
   * @param value value
   */
  public static void setScheduleOption(String jobUUID, String scheduleUUID, String name, String value)
  {
    try
    {
      executeCommand(StringParser.format("SCHEDULE_OPTION_SET jobUUID=%s scheduleUUID=%s name=%S value=%S",
                                         jobUUID,
                                         scheduleUUID,
                                         name,
                                         value
                                        ),
                     0  // debugLevel
                    );
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** get server option value from BAR server
   * @param name name of value
   * @param clazz type class
   * @return value
   */
  public static <T> T getServerOption(String name, Class clazz)
  {
    T data = null;

    try
    {
      ValueMap resultMap  = new ValueMap();
      executeCommand(StringParser.format("SERVER_OPTION_GET name=%S",name),
                     0,
                     resultMap
                    );
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
    catch (BARException exception)
    {
      // ignored
    }

    return data;
  }

  /** get server option value from BAR server
   * @param widgetVariable widget variable
   * @param clazz type class
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
   */
  public static void setServerOption(String name, boolean value)
    throws BARException
  {
    executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                       name,
                                       value ? "yes" : "no"
                                      ),
                   0  // debugLevel
                  );
  }

  /** set long option value on BAR server
   * @param name option name of value
   * @param value value
   */
  public static void setServerOption(String name, long value)
    throws BARException
  {
    executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%d",
                                       name,
                                       value
                                      ),
                   0  // debugLevel
                  );
  }

  /** set string option value on BAR server
   * @param name option name of value
   * @param value value
   */
  public static void setServerOption(String name, String value)
    throws BARException
  {
    executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%S",
                                       name,
                                       value
                                      ),
                   0  // debugLevel
                  );
  }

  /** get server option value on BAR server
   * @param widgetVariable widget variable
   */
  public static void getServerOption(WidgetVariable widgetVariable)
  {
    try
    {
      ValueMap valueMap = new ValueMap();
      executeCommand(StringParser.format("SERVER_OPTION_GET name=%S",widgetVariable.getName()),
                     0,  // debugLevel
                     valueMap
                    );

      if      (widgetVariable.getType() == Boolean.class)
      {
        widgetVariable.set(valueMap.getBoolean("value"));
      }
      else if (widgetVariable.getType() == Integer.class)
      {
        widgetVariable.set(valueMap.getInt("value"));
      }
      else if (widgetVariable.getType() == Long.class)
      {
        widgetVariable.set(valueMap.getLong("value"));
      }
      else if (widgetVariable.getType() == Double.class)
      {
        widgetVariable.set(valueMap.getDouble("value"));
      }
      else if (widgetVariable.getType() == String.class)
      {
        widgetVariable.set(valueMap.getString("value"));
      }
      else if (widgetVariable.getType() == Enum.class)
      {
  //        widgetVariable.set(valueMap.getString("value"));
  throw new Error("NYI");
      }
      else
      {
        throw new Error("Type not supported");
      }
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** set server option value on BAR server
   * @param widgetVariable widget variable
   */
  public static void setServerOption(WidgetVariable widgetVariable)
  {
    try
    {
      if      (widgetVariable.getType() == Boolean.class)
      {
        executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                           widgetVariable.getName(),
                                           widgetVariable.getBoolean() ? "yes" : "no"
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Integer.class)
      {
        executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%d",
                                           widgetVariable.getName(),
                                           widgetVariable.getInteger()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Long.class)
      {
        executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%ld",
                                           widgetVariable.getName(),
                                           widgetVariable.getLong()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Double.class)
      {
        executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%f",
                                           widgetVariable.getName(),
                                           widgetVariable.getDouble()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == String.class)
      {
        executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%'S",
                                           widgetVariable.getName(),
                                           widgetVariable.getString()
                                          ),
                       0  // debugLevel
                      );
      }
      else if (widgetVariable.getType() == Enum.class)
      {
  /*
          executeCommand(StringParser.format("SERVER_OPTION_SET name=%S value=%s",
                                               widgetVariable.getName(),
                                               widgetVariable.getLong()
                                              ),
                         0  // debugLevel
                          );
          break;*/
          throw new Error("NYI");
      }
      else
      {
        throw new Error("Type not supported");
      }
    }
    catch (BARException exception)
    {
      // ignored
    }
  }

  /** flush option values on BAR server
   */
  public static void flushServerOption()
    throws BARException
  {
    executeCommand(StringParser.format("SERVER_OPTION_FLUSH"),
                   0  // debugLevel
                  );
  }

  /** get password encrypt type
   * @return type
   */
  public static String getPasswordEncryptType()
  {
    return passwordEncryptType;
  }

  /** encrypt password as base64-string
   * @param password password
   * @return base64-string
   */
  public static String encryptPassword(String password)
    throws CommunicationError
  {
    byte[] encryptedPasswordBytes = new byte[0];

    if (password != null)
    {
      // get encoded password (XOR with session id)
      byte[] encodedPasswordBytes = new byte[sessionId.length];
      try
      {
        byte[] passwordBytes = password.getBytes("UTF-8");
        for (int i = 0; i < sessionId.length; i++)
        {
          if (i < passwordBytes.length)
          {
            encodedPasswordBytes[i] = (byte)((int)passwordBytes[i] ^ (int)sessionId[i]);
          }
          else
          {
            encodedPasswordBytes[i] = sessionId[i];
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
        encryptedPasswordBytes = passwordCipher.doFinal(encodedPasswordBytes);
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

    // encode as base64-string
    return "base64:"+base64Encode(encryptedPasswordBytes);
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
      return "RemoteFile {"+getAbsolutePath()+", "+fileType+", "+size+", "+dateTime+"}";
    }
  };

  /** list remote directory
   */
  public static ListDirectory<RemoteFile> remoteListDirectory = new ListDirectory<RemoteFile>()
  {
    private ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
    private Iterator<ValueMap>  iterator;

    /** get new file instance
     * @param name name
     * @return file
     */
    @Override
    public RemoteFile newFileInstance(String name)
    {
      FileTypes fileType = FileTypes.FILE;
      long      size     = 0;
      long      dateTime = 0;

      try
      {
        ValueMap valueMap = new ValueMap();
        BARServer.executeCommand(StringParser.format("FILE_INFO name=%'S",
                                                     name
                                                    ),
                                 1,  // debugLevel
                                 valueMap
                                );

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
      catch (BARException exception)
      {
        // ignored
      }

      return new RemoteFile(name,fileType,size,dateTime);
    }

    /** get shortcut files
     * @return shortcut files
     */
    @Override
    public void getShortcuts(List<RemoteFile> shortcutList)
    {
      final HashMap<String,RemoteFile> shortcutMap = new HashMap<String,RemoteFile>();

      // add manual shortcuts
      for (String name : Settings.shortcuts)
      {
        shortcutMap.put(name,new RemoteFile(name,FileTypes.DIRECTORY));
      }

      // add root shortcuts
      try
      {
        BARServer.executeCommand(StringParser.format("ROOT_LIST"),
                                 1,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     String name = valueMap.getString("name");
                                     long   size = Long.parseLong(valueMap.getString("size"));

                                     shortcutMap.put(name,new RemoteFile(name,size));
                                   }
                                 }
                                );
      }
      catch (final BARException exception)
      {
        // ignored
      }

      // create sorted list
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
    @Override
    public void addShortcut(RemoteFile shortcut)
    {
      Settings.shortcuts.add(shortcut.getAbsolutePath());
    }

    /** remove shortcut file
     * @param shortcut shortcut file
     */
    @Override
    public void removeShortcut(RemoteFile shortcut)
    {
      Settings.shortcuts.remove(shortcut.getAbsolutePath());
    }

    /** open list files in directory
     * @param pathName path name
     * @return true iff open
     */
    @Override
    public boolean open(RemoteFile path)
    {
      valueMapList.clear();
      try
      {
        BARServer.executeCommand(StringParser.format("FILE_LIST directory=%'S",
                                                     path.getAbsolutePath()
                                                    ),
                                 1,  // debugLevel
                                 valueMapList
                                );

        iterator = valueMapList.listIterator();
        return true;
      }
      catch (BARException exception)
      {
        iterator = null;
        return false;
      }
    }

    /** close list files in directory
     */
    @Override
    public void close()
    {
      iterator = null;
    }

    /** get next entry in directory
     * @return entry
     */
    @Override
    public RemoteFile getNext()
    {
      RemoteFile file = null;

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

  /** check if valid base64 character
   * @param ch characters
   * @return true iff valid
   */
  private static boolean validBase64Char(char ch)
  {
    return (   (((ch) >= 'A') && ((ch) <= 'Z'))
            || (((ch) >= 'a') && ((ch) <= 'z'))
            || (((ch) >= '0') && ((ch) <= '9'))
            || ((ch) == '+')
            || ((ch) == '/')
           );
  }

  /** decode base64-string
   * @param s base64-string
   * @return bytes
   */
  private static byte[] base64Decode(String s)
  {
    final byte BASE64_DECODING_TABLE[] =
    {
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,62,0,0,0,63,
      52,53,54,55,56,57,58,59,
      60,61,0,0,0,0,0,0,
      0,0,1,2,3,4,5,6,
      7,8,9,10,11,12,13,14,
      15,16,17,18,19,20,21,22,
      23,24,25,0,0,0,0,0,
      0,26,27,28,29,30,31,32,
      33,34,35,36,37,38,39,40,
      41,42,43,44,45,46,47,48,
      49,50,51,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,
    };

    int  n;
    byte data[];
    int  length;
    char c0,c1,c2,c3;
    int  i0,i1,i2,i3;
    int  i;
    byte b0,b1,b2;

    n = s.length();

    data = new byte[(n*3)/4];

    length = 0;
    c0 = 0;
    c1 = 0;
    c2 = 0;
    c3 = 0;
    i0 = 0;
    i1 = 0;
    i2 = 0;
    i3 = 0;
    i  = 0;
    while ((i+4) < n)
    {
      c0 = s.charAt(i+0);
      c1 = s.charAt(i+1);
      c2 = s.charAt(i+2);
      c3 = s.charAt(i+3);

      if (!validBase64Char(c0)) return null;
      if (!validBase64Char(c1)) return null;

      if      ((c2 == '=') && (c3 == '='))
      {
        // 1 byte
        i0 = BASE64_DECODING_TABLE[(byte)c0];
        i1 = BASE64_DECODING_TABLE[(byte)c1];

        b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));

        data[length] = b0; length++;
      }
      else if (c3 == '=')
      {
        // 2 bytes
        if (!validBase64Char(c2)) return null;

        i0 = BASE64_DECODING_TABLE[(byte)c0];
        i1 = BASE64_DECODING_TABLE[(byte)c1];
        i2 = BASE64_DECODING_TABLE[(byte)c2];

        b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));
        b1 = (byte)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));

        data[length] = b0; length++;
        data[length] = b1; length++;
      }
      else
      {
        // 3 bytes
        if (!validBase64Char(c2)) return null;
        if (!validBase64Char(c3)) return null;

        i0 = BASE64_DECODING_TABLE[(byte)c0];
        i1 = BASE64_DECODING_TABLE[(byte)c1];
        i2 = BASE64_DECODING_TABLE[(byte)c2];
        i3 = BASE64_DECODING_TABLE[(byte)c3];

        b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));
        b1 = (byte)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));
        b2 = (byte)(((i2 & 0x03) << 6) | i3);

        data[length] = b0; length++;
        data[length] = b1; length++;
        data[length] = b2; length++;
      }

      i += 4;
    }

    return data;
  }

  /** encode base64-string
   * @param data bytes
   * @return base64-string
   */
  private static String base64Encode(byte data[])
  {
    final char BASE64_ENCODING_TABLE[] =
    {
      'A','B','C','D','E','F','G','H',
      'I','J','K','L','M','N','O','P',
      'Q','R','S','T','U','V','W','X',
      'Y','Z','a','b','c','d','e','f',
      'g','h','i','j','k','l','m','n',
      'o','p','q','r','s','t','u','v',
      'w','x','y','z','0','1','2','3',
      '4','5','6','7','8','9','+','/'
    };

    StringBuilder stringBuffer = new StringBuilder(data.length*2);
    int           i;
    byte          b0,b1,b2;
    int           i0,i1,i2,i3;

    // encode 3-byte tupels
    i = 0;
    while ((i+2) < data.length)
    {
      b0 = data[i+0];
      b1 = data[i+1];
      b2 = data[i+2];

      i0 = (int)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (int)((b0 & 0x03) << 4) | (int)((b1 & 0xF0) >> 4);
      assert(i1 < 64);
      i2 = (int)((b1 & 0x0F) << 2) | (int)((b2 & 0xC0) >> 6);
      assert(i2 < 64);
      i3 = (int)(b2 & 0x3F);
      assert(i3 < 64);

      stringBuffer.append(BASE64_ENCODING_TABLE[i0]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i1]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i2]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i3]);

      i += 3;
    }

    // encode last 1,2 bytes
    if      ((i+1) >= data.length)
    {
      // 1 byte => XY==
      b0 = data[i+0];

      i0 = (int)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (int)((b0 & 0x03) << 4);
      assert(i1 < 64);

      stringBuffer.append(BASE64_ENCODING_TABLE[i0]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i1]);
      stringBuffer.append('=');
      stringBuffer.append('=');
    }
    else if  ((i+2) >= data.length)
    {
      // 2 byte => XYZ=
      b0 = data[i+0];
      b1 = data[i+1];

      i0 = (int)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (int)((b0 & 0x03) << 4) | (int)((b1 & 0xF0) >> 4);
      assert(i1 < 64);
      i2 = (int)((b1 & 0x0F) << 2);
      assert(i2 < 64);

      stringBuffer.append(BASE64_ENCODING_TABLE[i0]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i1]);
      stringBuffer.append(BASE64_ENCODING_TABLE[i2]);
      stringBuffer.append('=');
    }

    return stringBuffer.toString();
  }

  /** decode hex string
   * @param s hex string
   * @return bytes
   */
  private static byte[] hexDecode(String s)
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
  private static String hexEncode(byte data[])
  {
    StringBuilder stringBuffer = new StringBuilder(data.length*2);
    for (int z = 0; z < data.length; z++)
    {
      stringBuffer.append(String.format("%02x",(int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  /** accept session: read session id, password encryption type and key
   * @param input,output input/output streams
   */
  private static void acceptSession(BufferedReader input, BufferedWriter output)
    throws IOException
  {
    sessionId           = null;
    passwordEncryptType = null;
    passwordCipher      = null;
    mode                = Modes.MASTER;

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
    logReceived(1,"%s",line);
    data = line.split(" ",2);
    if ((data.length < 2) || !data[0].equals("SESSION"))
    {
      throw new CommunicationError("Invalid response from server: expected SESSION");
    }
    if (!StringParser.parse(data[1],valueMap))
    {
      throw new CommunicationError("Invalid response from server: expected parameters");
    }
    sessionId = hexDecode(valueMap.getString("id"));
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
//Dprintf.dprintf("n=%s -> %s e=%s -> %s",valueMap.getString("n"),n,valueMap.getString("e"),e);

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
   * @param debugLevel debug level (0..n)
   * @param valueMap values or null
   */
  public static void syncExecuteCommand(BufferedReader input,
                                        BufferedWriter output,
                                        String         commandString,
                                        int            debugLevel,
                                        ValueMap       valueMap
                                       )
    throws IOException,BARException
  {
    int errorCode;

    synchronized(lock)
    {
      // get new command
      Command command = new Command(commandString,0);

      // send command
      String line = String.format("%d %s",command.id,command.string);
      output.write(line); output.write('\n'); output.flush();
      logSent(debugLevel,"%s",line);

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
        logReceived(debugLevel,"%s",line);

        // parse
        data = line.split(" ",4);
        if (data.length < 3) // at least 3 values: <command id> <complete flag> <error code>
        {
          throw new CommunicationError("Invalid response from server: incomplete response '"+line+"'");
        }
      }
      while (Integer.parseInt(data[0]) != command.id);

      // check result
      if (Integer.parseInt(data[1]) != 1)
      {
        throw new CommunicationError("Invalid response from server: no command id");
      }

      // get result
      errorCode = Integer.parseInt(data[2]);
      if (errorCode == BARException.NONE)
      {
        if (valueMap != null)
        {
          valueMap.clear();
          if (!StringParser.parse(data[3],valueMap))
          {
            throw new CommunicationError("Invalid response from server: parameters");
          }
        }
      }
      else
      {
        throw new BARException(errorCode,data[3]);
      }
    }
  }

  /** execute command syncronous
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   * @param valueMap values or null
   */
  public static void syncExecuteCommand(String   commandString,
                                        int      debugLevel,
                                        ValueMap valueMap
                                       )
    throws IOException,BARException
  {
    syncExecuteCommand(input,output,commandString,debugLevel,valueMap);
  }

  /** execute command syncronous
   * @param input,output input/output streams
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   */
  public static void syncExecuteCommand(BufferedReader input,
                                        BufferedWriter output,
                                        String         commandString,
                                        int            debugLevel
                                       )
    throws IOException,BARException
  {
    syncExecuteCommand(input,output,commandString,debugLevel,(ValueMap)null);
  }

  /** execute command syncronous
   * @param commandString command string
   * @param debugLevel debug level (0..n)
   */
  public static void syncExecuteCommand(String commandString,
                                        int    debugLevel
                                       )
    throws IOException,BARException
  {
    syncExecuteCommand(commandString,debugLevel,(ValueMap)null);
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

  /** enque command to process
   * @param id command id
   * @param name command name
   * @param valueMap command value map
   */
  public static void process(long id, String name, ValueMap valueMap)
  {
    commandThread.process(id,name,valueMap);
  }

  /** log sent data
   * @param debugLevel debug level
   * @param format format string
   * @param arguments optional arguments
   */
  public static void logSent(int debugLevel, String format, Object... arguments)
  {
    logTime0 = new Date();
    if (Settings.debugLevel >= debugLevel)
    {
      System.err.println("Network sent     "+BARServer.getLogTimeInfo()+": '"+String.format(format,arguments)+"'");
    }
  }

  /** log received data
   * @param debugLevel debug level
   * @param format format string
   * @param arguments optional arguments
   */
  public static void logReceived(int debugLevel, String format, Object... arguments)
  {
    if (Settings.debugLevel >= debugLevel)
    {
      System.err.println("Network received "+BARServer.getLogTimeInfo()+": '"+String.format(format,arguments)+"'");
    }
  }

  // -------------------------------------------------------------------

  /** get log time info
   * @return log time info
   */
  private static String getLogTimeInfo()
  {
    Date   logTime1 = new Date();
    long   duration = logTime1.getTime()-logTime0.getTime();
    String timeInfo;

    timeInfo = String.format("%s %3dms",LOG_TIME_FORMAT.format(logTime1),duration);
    logTime0 = logTime1;
    if (duration > 100)
    {
      System.err.println("Network warning  "+BARServer.getLogTimeInfo()+": long duration "+duration+"ms > 100ms!");
    }

    return timeInfo;
  }
}

/* end of file */
