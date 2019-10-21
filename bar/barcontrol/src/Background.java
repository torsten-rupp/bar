/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: background task
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.FutureTask;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

import org.eclipse.swt.SWTException;

/****************************** Classes ********************************/

/** background task runnable
 */
abstract class BackgroundRunnable implements Runnable
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  protected Object[]            userData;

  private   StackTraceElement[] stackTrace;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create background task
   * @param userData user data
   */
  BackgroundRunnable(Object... userData)
  {
    this.userData   = userData;
    this.stackTrace = Thread.currentThread().getStackTrace();
  }

  /** default run method: output error
   */
  public void run()
  {
    BARControl.printInternalError("No run method declared for background task!");
    System.err.println("Stack trace:");
    for (int z = 2; z < stackTrace.length; z++)
    {
      System.err.println("  "+stackTrace[z]);
    }
    System.exit(ExitCodes.INTERNAL_ERROR);
  }

  /** call to abort execution
   */
  public void abort()
  {
  }
}

/** background task
 */
class BackgroundTask implements Runnable
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private BackgroundRunnable backgroundRunnable;
  private Method             runMethod;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create background task
   * @param backgroundRunnable background runnable to execute
   */
  BackgroundTask(BackgroundRunnable backgroundRunnable)
  {
    this.backgroundRunnable = backgroundRunnable;
    this.runMethod          = null;

    // find matching run-method if possible
    for (Method method : backgroundRunnable.getClass().getDeclaredMethods())
    {
      // check name
      if (method.getName().equals("run"))
      {
        // check parameters
        Class[] parameterTypes = method.getParameterTypes();
        if (parameterTypes.length == backgroundRunnable.userData.length)
        {
          boolean match = true;
          for (int i = 0; (i < backgroundRunnable.userData.length) && match; i++)
          {
            if (backgroundRunnable.userData[i] != null)
            {
              match = parameterTypes[i].isAssignableFrom(backgroundRunnable.userData[i].getClass());
            }
            else
            {
              match = Object.class.isAssignableFrom(parameterTypes[i]);
            }
          }

          if (match)
          {
            runMethod = method;
            break;
          }
        }
      }
    }
  }

  /** run background runnable
   */
  public void run()
  {
    try
    {
      if (runMethod != null)
      {
        // call specific run-method
        runMethod.invoke(backgroundRunnable,backgroundRunnable.userData);
      }
      else
      {
        // call general run-method
        backgroundRunnable.run();
      }
    }
    catch (InvocationTargetException exception)
    {
      BARControl.printError("Unhandled background exception: %s",exception.getCause());
      System.err.println("Stack trace:");
      BARControl.printStackTrace(exception);
      System.exit(ExitCodes.INTERNAL_ERROR);
    }
    catch (SWTException exception)
    {
      BARControl.printError("Unhandled SWT background exception: %s",exception);
      System.err.println("Stack trace:");
      BARControl.printStackTrace(exception);
      System.exit(ExitCodes.INTERNAL_ERROR);
    }
    catch (Exception exception)
    {
      BARControl.printError("Unhandled background exception: %s",exception);
      System.err.println("Stack trace:");
      BARControl.printStackTrace(exception);
    }
    catch (Error exception)
    {
      BARControl.printError("Unhandled background error: %s",exception);
      System.err.println("Stack trace:");
      BARControl.printStackTrace(exception);
    }
  }
}

/** background executor
 */
public class Background
{
  // --------------------------- constants --------------------------------
  private final static int MAX_BACKGROUND_TASKS = 8;

  // --------------------------- variables --------------------------------

  public static ExecutorService executorService = Executors.newFixedThreadPool(MAX_BACKGROUND_TASKS,new ThreadFactory()
  {
    public Thread newThread(Runnable runnable)
    {
      Thread thread = new Thread(runnable);
      thread.setDaemon(true);

      return thread;
    }
  });

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** run task in background
   * @param backgroundRunnable task to run in background
   */
  public static void run(BackgroundRunnable backgroundRunnable)
  {
    executorService.submit(new BackgroundTask(backgroundRunnable));
  }

  /** shutdown background tasks
   */
  public static void shutdown()
  {
    executorService.shutdown();
    try
    {
      executorService.awaitTermination(5000,TimeUnit.MILLISECONDS);
    }
    catch (InterruptedException exception)
    {
      // ignored
    }
  }
}

/* end of file */
