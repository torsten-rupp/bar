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

import java.lang.RuntimeException;

import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.NoRouteToHostException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;

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
  public boolean endOfData()
  {
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
          /* ignored */
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
  public synchronized boolean waitCompleted(long timeout)
  {
    while (   !completedFlag
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
          /* ignored */
        }
      }
      else
      {
        // overall timeout
        timeout();
      }
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

  /** get next result
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
        /* ignored */
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
          if (Settings.serverDebugFlag) System.err.println("Network: received '"+line+"'");

          // parse: line format <id> <error code> <completed flag> <data>
          String data[] = line.split(" ",4);
          if (data.length < 4)
          {
            throw new CommunicationError("malformed command result '"+line+"'");
          }

          // get command id, completed flag, error code
          long    commandId     = Long.parseLong(data[0]);
          boolean completedFlag = (Integer.parseInt(data[1]) != 0);;
          int     errorCode     = Integer.parseInt(data[2]);

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
                      command.processResult.process(data[3]);
                    }
                    else
                    {
                      command.result.add(data[3]);
                    }
                  }
                  else
                  {
                    command.errorText = data[3];
                  }
                  command.completedFlag = true;
                  command.notifyAll();
                }
                else
                {
                  command.errorCode = Errors.NONE;
                  if (command.processResult != null)
                  {
                    command.processResult.process(data[3]);
                  }
                  else
                  {
                    command.result.add(data[3]);
                    command.notifyAll();
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
        /* communication impossible, cancel all commands with error and wait for termination */
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
  public final static  String JAVA_SSL_KEY_FILE_NAME = "bar.jks";  // default name Java TLS/SSL key

  public static char fileSeparator;

  private final static int    SOCKET_READ_TIMEOUT    = 20*1000;    // timeout reading socket [ms]

  // --------------------------- variables --------------------------------
  private static long               commandId;
  private static Socket             socket;
  private static BufferedWriter     output;
  private static BufferedReader     input;
  private static ReadThread         readThread;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

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
    String errorMessage = null;
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
            errorMessage = "host '"+hostname+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (ConnectException exception)
          {
            errorMessage = "host '"+hostname+"' unreachable (error: "+exception.getMessage()+")";
          }
          catch (NoRouteToHostException exception)
          {
            errorMessage = "host '"+hostname+"' unreachable (no route to host)";
          }
          catch (UnknownHostException exception)
          {
            errorMessage = "unknown host '"+hostname+"'";
          }
          catch (RuntimeException exception)
          {
            errorMessage = exception.getMessage();
          }
          catch (Exception exception)
          {
            errorMessage = exception.getMessage();
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
        errorMessage = exception.getMessage();
      }
      catch (ConnectException exception)
      {
        errorMessage = exception.getMessage();
      }
      catch (NoRouteToHostException exception)
      {
        errorMessage = "host '"+hostname+"' unreachable (no route to host)";
      }
      catch (UnknownHostException exception)
      {
        errorMessage = "unknown host '"+hostname+"'";
      }
      catch (Exception exception)
      {
//        exception.printStackTrace();
        errorMessage = exception.getMessage();
      }
    }
    if (socket == null)
    {
      if   ((tlsPort != 0) || (port!= 0)) throw new ConnectionError(errorMessage);
      else                                throw new ConnectionError("no server ports specified");
    }

    try
    {
      String   line;
      String[] data;

      // read session id
      byte sessionId[];
      line = input.readLine();
      if (line == null)
      {
        throw new CommunicationError("No result from server");
      }
      if (Settings.serverDebugFlag) System.err.println("Network: received '"+line+"'");
      data = line.split(" ",2);
      if ((data.length < 2) || !data[0].equals("SESSION"))
      {
        throw new CommunicationError("Invalid response from server");
      }
      sessionId = decodeHex(data[1]);
//System.err.print("BARControl.java"+", "+682+": sessionId=");for (byte b : sessionId) { System.err.print(String.format("%02x",b & 0xFF)); }; System.err.println();

      // authorize
      byte authorizeData[] = new byte[sessionId.length];
      for (int z = 0; z < sessionId.length; z++)
      {
        authorizeData[z] = (byte)(((z < serverPassword.length())?(int)serverPassword.charAt(z):0)^(int)sessionId[z]);
      }
      commandId++;
      line = Long.toString(commandId)+" AUTHORIZE "+encodeHex(authorizeData);
      output.write(line); output.write('\n'); output.flush();
      if (Settings.serverDebugFlag) System.err.println("Network: sent '"+line+"'");
      line = input.readLine();
      if (line == null)
      {
        throw new CommunicationError("No result from server");
      }
      if (Settings.serverDebugFlag) System.err.println("Network: received '"+line+"'");
      data = line.split(" ",4);
      if (data.length < 3) // at least 3 values: <command id> <complete flag> <error code>
      {
        throw new CommunicationError("Invalid response from server");
      }
      if (   (Integer.parseInt(data[0]) != commandId)
          || (Integer.parseInt(data[1]) != 1)
          || (Integer.parseInt(data[2]) != 0)
         )
      {
        throw new ConnectionError("Authorization fail");
      }

      // get version
      line = "VERSION";
      output.write(line); output.write('\n'); output.flush();
      if (Settings.serverDebugFlag) System.err.println("Network: sent '"+line+"'");
      line = input.readLine();
      if (line == null)
      {
        throw new CommunicationError("No result from server");
      }
      if (Settings.serverDebugFlag) System.err.println("Network: received '"+line+"'");
      data = line.split(" ",5);
      if (data.length != 5) // exactly 5 values: <command id> <complete flag> <error code> <major version> <minor version>
      {
        throw new CommunicationError("Invalid response from server");
      }
      if (   (Integer.parseInt(data[1]) != 1)
          || (Integer.parseInt(data[2]) != 0)
         )
      {
        throw new ConnectionError("Cannot get protocol version for '"+hostname+"' (error: "+data[3]+")");
      }
      if (Integer.parseInt(data[3]) != 1)
      {
        throw new CommunicationError("Incompatible protocol version for '"+hostname+"' (expected 1, got "+data[3]+")");
      }

      // get file separator character
      commandId++;
      line = Long.toString(commandId)+" GET FILE_SEPARATOR";
      output.write(line); output.write('\n'); output.flush();
      if (Settings.serverDebugFlag) System.err.println("Network: sent '"+line+"'");
      line = input.readLine();
      if (line == null)
      {
        throw new CommunicationError("No result from server");
      }
      if (Settings.serverDebugFlag) System.err.println("Network: received '"+line+"'");
      data = line.split(" ",4);
      if (data.length < 4) // at least 4 values: <command id> <complete flag> <error code> <separator char>|<error text>
      {
        throw new CommunicationError("Invalid response from server");
      }
      if (   (Integer.parseInt(data[0]) != commandId)
          || (Integer.parseInt(data[1]) != 1)
          || (Integer.parseInt(data[2]) != 0)
         )
      {
        throw new ConnectionError("Get file separator character fail (error: "+data[3]+")");
      }
      fileSeparator = data[3].charAt(0);
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error on "+socket.getInetAddress()+":"+socket.getPort()+" (error: "+exception.getMessage()+")");
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

      // stop read thread
      readThread.quit();
      try { readThread.join(); } catch (InterruptedException exception) { /* ignored */ }

      // close connection, wait until read thread stopped
      socket.close();
      try
      {
        readThread.join();
      }
      catch (InterruptedException exception)
      {
      }

      // free resources
      input.close();
      output.close();
    }
    catch (IOException exception)
    {
      // ignored
    }
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
        if (Settings.serverDebugFlag) System.err.println("Network: sent '"+line+"'");
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
//        throw new CommunicationError("i/o error on "+socket.getInetAddress()+":"+socket.getPort()+" (error: "+exception.getMessage()+")");
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
    executeCommand(String.format("ABORT %d",command.id));
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
    executeCommand(String.format("ABORT %d",command.id));
    readThread.commandRemove(command);

    // set abort error
    command.errorCode     = -3;
    command.errorText     = "timeout";
    command.completedFlag = true;
    command.result.clear();
  }

  /** execute command
   * @param command command to send to BAR server
   * @param result result (String[] or ArrayList)
   * @param busyIndicator busy indicator or null
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String commandString, Object result, BusyIndicator busyIndicator)
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
        if (Settings.serverDebugFlag) System.err.println("Network: sent '"+line+"'");
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
    if      (result == null)
    {
      errorCode = command.getResult();
    }
    else if (result instanceof ArrayList)
    {
      errorCode = command.getResult((ArrayList<String>)result);
    }
    else if (result instanceof String[])
    {
      errorCode = command.getResult((String[])result);
    }
    else
    {
      throw new Error("Invalid result data type");
    }

    // free command
    readThread.commandRemove(command);

    return errorCode;
  }

  /** execute command
   * @param command command to send to BAR server
   * @param result result (String[] or ArrayList)
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String command, Object result)
  {
    return executeCommand(command,result,null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @return Errors.NONE or error code
   */
  public static int executeCommand(String command)
  {
    return executeCommand(command,null);
  }

  /** set boolean value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param b value
   */
  public static void set(String name, boolean b)
  {
    executeCommand("SET "+name+" "+(b?"yes":"no"));
  }

  /** set long value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param n value
   */
  static void set(String name, long n)
  {
    executeCommand("SET "+name+" "+n);
  }

  /** set string value on BAR server
   * @param jobId job id
   * @param name name of value
   * @param s value
   */
  public static void set(String name, String s)
  {
    executeCommand("SET "+name+" "+StringUtils.escape(s));
  }

  /** get boolean value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static boolean getBooleanOption(int jobId, String name)
  {
    String[] result = new String[1];

    if (executeCommand("OPTION_GET "+jobId+" "+name,result) == Errors.NONE)
    {
      return    result[0].toLowerCase().equals("yes")
             || result[0].toLowerCase().equals("on")
             || result[0].equals("1");
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
    String[] result = new String[1];

    if (executeCommand("OPTION_GET "+jobId+" "+name,result) == Errors.NONE)
    {
      return Long.parseLong(result[0]);
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
    String[] result = new String[1];

    if (executeCommand("OPTION_GET "+jobId+" "+name,result) == Errors.NONE)
    {
      return StringUtils.unescape(result[0]);
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
    executeCommand("OPTION_SET "+jobId+" "+name+" "+(b?"yes":"no"));
  }

  /** set long option value on BAR server
   * @param jobId job id
   * @param name option name of value
   * @param n value
   */
  public static void setOption(int jobId, String name, long n)
  {
    executeCommand("OPTION_SET "+jobId+" "+name+" "+n);
  }

  /** set string option value on BAR server
   * @param jobId job id
   * @param name option name of value
   * @param s value
   */
  public static void setOption(int jobId, String name, String s)
  {
    executeCommand("OPTION_SET "+jobId+" "+name+" "+StringUtils.escape(s));
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
}

/* end of file */
