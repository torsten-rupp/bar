/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BARServer.java,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
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

/** Connection error
 */
class ConnectionError extends Error
{
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
class Indicator
{
  /** called when busy
   */
  public boolean busy(long n)
  {
    return true;
  }
}

/** process result
 */
abstract class ProcessResult
{
  abstract public void process(String result);
}

/** BAR command
 */
class Command
{
  ProcessResult      processResult;
  long               id;
  int                errorCode;
  String             errorText;
  boolean            completedFlag;
  LinkedList<String> result;

  /** create new command
   * @param id command id
   * @param processResult process result handler
   */
  Command(long id, ProcessResult processResult)
  {
    this.id            = id;
    this.errorCode     = -1;
    this.errorText     = "";
    this.completedFlag = false;
    this.processResult = processResult;
    this.result        = new LinkedList<String>();
  }

  /** create new command
   * @param id command id
   */
  Command(long id)
  {
    this.id            = id;
    this.errorCode     = -1;
    this.errorText     = "";
    this.completedFlag = false;
    this.processResult = null;
    this.result        = new LinkedList<String>();
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
    while (!completedFlag && (result.size() == 0) && (timeout != 0))
    {
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
    while (!completedFlag && (timeout != 0))
    {
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
    if (!completedFlag && (result.size() == 0))
    {
      try
      {
        this.wait(timeout);
      }
      catch (InterruptedException exception)
      {
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
}

/** server result read thread
 */
class ReadThread extends Thread
{
  private BufferedReader        input;
  private boolean               quitFlag;
  private HashMap<Long,Command> commandHashMap = new HashMap<Long,Command>();

  boolean debug = false;

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
        line = input.readLine();
//Dprintf.dprintf("line=%s\n",line);
        if (line == null)
        {
          if (!quitFlag)
          {
            throw new CommunicationError("disconnected");
          }
          else
          {
            break;
          }
        }
        if (debug) System.err.println("Network: received '"+line+"'");

        // parse: line format <id> <error code> <completed flag> <data>
        String data[] = line.split(" ",4);
        if (data.length < 4)
        {
          throw new CommunicationError("malformed command result '"+line+"'");
        }
        try
        {
          long    commandId;
          int     errorCode;
          boolean completedFlag;

          // get command id, completed flag, error code
          commandId     = Long.parseLong(data[0]);
          completedFlag = (Integer.parseInt(data[1]) != 0);
          errorCode     = Integer.parseInt(data[2]);

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
Dprintf.dprintf("not found %d: %s\n",commandId,line);
            }
          }
        }
        catch (NumberFormatException exception)
        {
//          throw new CommunicationError("malformed command result '"+line+"'");
        }
      }
      catch (IOException exception)
      {
//        throw new CommunicationError("Command fail (error: "+exception.getMessage()+")");
        break;
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
   * @param processResult process result handler
   * @return command
   */
  public Command commandAdd(long commandId, ProcessResult processResult)
  {
    synchronized(commandHashMap)
    {
      Command command = new Command(commandId,processResult);
      commandHashMap.put(commandId,command);
      return command;
    }
  }

  /** add command
   * @param commandId command id
   * @return command
   */
  public Command commandAdd(long commandId)
  {
    return commandAdd(commandId,null);
  }

  /** remove command
   * @param command command to remove
   */
  public int commandRemove(Command command)
  {
    synchronized(commandHashMap)
    {
      commandHashMap.remove(command.id);
      return command.errorCode;
    }
  }
}

/** BAR server
 */
class BARServer
{
  public static boolean debug = false;

  private static String             JAVA_SSL_KEY_FILE_NAME = "bar.jks";

  private static long               commandId;
  private static Socket             socket;
  private static BufferedWriter     output;
  private static BufferedReader     input;
  private static ReadThread         readThread;

  /** connect to BAR server
   * @param hostname host name
   * @param port port number or 0
   * @param tlsPort TLS port number of 0
   * @param serverPassword server password
   */
  public static void connect(String hostname, int port, int tlsPort, String serverPassword)
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
      String[] javaSSLKeyFileNames = new String[]
      {
        JAVA_SSL_KEY_FILE_NAME,
        System.getProperty("user.home")+File.separator+".bar"+File.separator+JAVA_SSL_KEY_FILE_NAME,
        Config.CONFIG_DIR+File.separator+JAVA_SSL_KEY_FILE_NAME
      };

      // try to connect with key
      for (String javaSSLKeyFileName : javaSSLKeyFileNames)
      {
        File file = new File(javaSSLKeyFileName);
        if (file.exists() && file.isFile())
        {
          System.setProperty("javax.net.ssl.trustStore",javaSSLKeyFileName);
          try
          {
            SSLSocket        sslSocket;
            SSLSocketFactory sslSocketFactory;

            sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();
            sslSocket        = (SSLSocket)sslSocketFactory.createSocket(hostname,tlsPort);
            sslSocket.startHandshake();

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
    //        exception.printStackTrace();
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

    // read session id
    byte sessionId[];
    try
    {
      String line;

      line = input.readLine();
      assert line != null;
      String data[] = line.split(" ",2);
      assert data.length == 2;
      assert data[0].equals("SESSION");
      sessionId = decodeHex(data[1]);
    }
    catch (IOException exception)
    {
      throw new Error("Network error (error: "+exception.getMessage()+")");
    }
//System.err.print("BARControl.java"+", "+682+": sessionId=");for (byte b : sessionId) { System.err.print(String.format("%02x",b & 0xFF)); }; System.err.println();

    // authorize
    try
    {
      byte authorizeData[] = new byte[sessionId.length];
      for (int z = 0; z < sessionId.length; z++)
      {
        authorizeData[z] = (byte)(((z < serverPassword.length())?(int)serverPassword.charAt(z):0)^(int)sessionId[z]);
      }
      commandId++;
      String command = Long.toString(commandId)+" AUTHORIZE "+encodeHex(authorizeData);
      output.write(command); output.write('\n'); output.flush();

      String result = input.readLine();
      if (result == null)
      {
        throw new CommunicationError("No result from server");
      }
      String data[] = result.split(" ",4);
      if (data.length < 3)
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
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error (error: "+exception.getMessage()+")");
    }

    // get version
    try
    {
      String result;

      output.write("VERSION"); output.write('\n'); output.flush();
      result = input.readLine();
      if (result == null)
      {
        throw new CommunicationError("No result from server");
      }
      String data[] = result.split(" ",4);
      if (data.length < 3)
      {
        throw new CommunicationError("Invalid response from server");
      }
      if (   (Integer.parseInt(data[1]) != 1)
          || (Integer.parseInt(data[2]) != 0)
         )
      {
        throw new ConnectionError("Cannot connect to '"+hostname+"' (error: "+data[3]+")");
      }
    }
    catch (IOException exception)
    {
      throw new CommunicationError("Network error (error: "+exception.getMessage()+")");
    }

    // start read thread
    readThread = new ReadThread(input);
    readThread.debug = debug;
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
//output.write("QUIT"); output.write('\n'); output.flush();

      // close connection, wait until read thread stopped
      readThread.quit();
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

  public static Command runCommand(String commandString, ProcessResult processResult)
  {
    Command command;

    synchronized(output)
    {
      // new command
      commandId++;
      String line = String.format("%d %s",commandId,commandString);

      // add command
      command = readThread.commandAdd(commandId,processResult);

      // send command
      try
      {
        if (debug) System.err.println("Network: sent '"+line+"'");
        output.write(line); output.write('\n'); output.flush();
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
        throw new CommunicationError("i/o error (error: "+exception.getMessage()+")");
      }
    }

    return command;
  }

  public static Command runCommand(String commandString)
  {
    return runCommand(commandString,null);
  }

  /** abort command execution
   * @param command command to abort
   * @param result result (String[] or ArrayList)
   * @return error code
   */
  static void abortCommand(Command command)
  {
    // send abort command to command
    executeCommand(String.format("ABORT %d",command.id));
    readThread.commandRemove(command);

    // set abort error
    command.errorCode     = -1;
    command.errorText     = "aborted";
    command.completedFlag = true;
    command.result.clear();
  }

  /** execute command
   * @param command command to send to BAR server
   * @param result result (String[] or ArrayList)
   * @return 0 or error code
   */
  public static int executeCommand(String commandString, Object result, Indicator indicator)
  {
    Command command;
    int     errorCode;

    synchronized(output)
    {
      if (indicator != null)
      {
        if (!indicator.busy(0)) return -1;
      }

      // new command
      commandId++;
      String line = String.format("%d %s",commandId,commandString);

      // add command
      command = readThread.commandAdd(commandId);

      // send command
      try
      {
        if (debug) System.err.println("Network: sent '"+line+"'");
        output.write(line); output.write('\n'); output.flush();
      }
      catch (IOException exception)
      {
        readThread.commandRemove(command);
//        throw new CommunicationError("i/o error (error: "+exception.getMessage()+")");
        return -1;
      }
      if (indicator != null)
      {
        if (!indicator.busy(0))
        {
          abortCommand(command);
          return command.getErrorCode();
        }
      }
    }

    // wait until completed
    while (!command.waitCompleted(250))
    {
      if (indicator != null)
      {
        if (!indicator.busy(0))
        {
          command.abort();
          return command.getErrorCode();
        }
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
   * @return 0 or error code
   */
  public static int executeCommand(String command, Object result)
  {
    return executeCommand(command,result,null);
  }

  /** execute command
   * @param command command to send to BAR server
   * @return 0 or error code
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
    executeCommand("SET "+name+" "+StringParser.escape(s));
  }

  /** get boolean value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static boolean getBooleanOption(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return    result[0].toLowerCase().equals("yes")
           || result[0].toLowerCase().equals("on")
           || result[0].equals("1");
  }

  /** get long value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static long getLongOption(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return Long.parseLong(result[0]);
  }

  /** get string value from BAR server
   * @param jobId job id
   * @param name name of value
   * @return value
   */
  public static String getStringOption(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return StringParser.unescape(result[0]);
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
    executeCommand("OPTION_SET "+jobId+" "+name+" "+StringParser.escape(s));
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
