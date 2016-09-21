/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BusyDialog.java,v $
* $Revision: 1199 $
* $Author: trupp $
* Contents: busy dialog
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.text.MessageFormat;

import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

import org.xnap.commons.i18n.I18n;
import org.xnap.commons.i18n.I18nFactory;

/****************************** Classes ********************************/

/** busy dialog
 */
class BusyDialog
{
  // --------------------------- constants --------------------------------

  // busy dialog flags
  public final static int NONE          = 0;
  public final static int TEXT0         = 1 <<  0;
  public final static int TEXT1         = 1 <<  1;
  public final static int PROGRESS_BAR0 = 1 <<  2;
  public final static int PROGRESS_BAR1 = 1 <<  3;
  public final static int LIST          = 1 <<  4;
  public final static int ABORT_CLOSE   = 1 <<  5;

  public final static int AUTO_ANIMATE  = 1 << 24;

  // --------------------------- variables --------------------------------
  private static I18n i18n;

  private Display     display;

  private int         animateInterval = 100;  // [ms]
  private long        animateTimestamp;
  private Image       animateImages[] = new Image[2];
  private int         animateImageIndex;
  private boolean     animateQuitFlag;

  private Shell       dialog;
  private Label       widgetImage;
  private Label       widgetMessage;
  private Label       widgetText0,widgetText1,widgetText2;
  private ProgressBar widgetProgressBar0,widgetProgressBar1;
  private List        widgetList;
  private Button      widgetAbortCloseButton;
  private int         maxListLength;

  private boolean     doneFlag;
  private boolean     abortedFlag;
  private boolean     resizedFlag;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** init internationalization
   * @param i18n internationlization instance
   */
  public static void init(I18n i18n)
  {
    BusyDialog.i18n = i18n;
  }

  /** get internationalized text
   * @param text text
   * @param arguments text
   * @return internationalized text
   */
  public static String tr(String text, Object... arguments)
  {
    if (i18n != null)
    {
      return i18n.tr(text,arguments);
    }
    else
    {
      return MessageFormat.format(text,arguments);
    }
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   * @param maxListLength max. list length
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message, final int flags, int maxListLength)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite,subComposite;
    Label           label;
    int             x,y;

    this.maxListLength = maxListLength;

    display = parentShell.getDisplay();

    // load animation images
    animateImages[0] = Widgets.loadImage(display,"busy0.png");
    animateImages[1] = Widgets.loadImage(display,"busy1.png");
    animateTimestamp  = System.currentTimeMillis();
    animateImageIndex = 0;
    animateQuitFlag   = false;

    // create dialog
    dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    dialog.setText(title);
    tableLayout = new TableLayout(new double[]{1.0,0.0},1.0);
    tableLayout.minWidth  = width;
    tableLayout.minHeight = height;
    dialog.setLayout(tableLayout);
    dialog.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message/progress bar/list
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      widgetImage = new Label(composite,SWT.LEFT);
      widgetImage.setImage(animateImages[animateImageIndex]);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,4));

      double[] rowWeights = new double[7];
      int row = 0;
      if (message != null)
      {
        rowWeights[row] = 0.0; row++;
      }
      if ((flags & TEXT0) != 0)
      {
        rowWeights[row] = ((flags & LIST) == 0) ? 1.0 : 0.0; row++;
      }
      if ((flags & PROGRESS_BAR0) != 0)
      {
        rowWeights[row] = 0.0; row++;
      }
      if ((flags & TEXT1) != 0)
      {
        rowWeights[row] = ((flags & LIST) == 0) ? 1.0 : 0.0; row++;
      }
      if ((flags & PROGRESS_BAR1) != 0)
      {
        rowWeights[row] = 0.0; row++;
      }
      if ((flags & LIST) != 0)
      {
        rowWeights[row] = 0.0; row++;
        rowWeights[row] = 1.0; row++;
      }

      subComposite = new Composite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(rowWeights,1.0));
      subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
      {
        row = 0;

        if (message != null)
        {
          widgetMessage = new Label(subComposite,SWT.LEFT);
          widgetMessage.setText(message);
          widgetMessage.setLayoutData(new TableLayoutData(row,0,TableLayoutData.N|TableLayoutData.WE)); row++;
        }
        else
        {
          widgetMessage = null;
        }

        if ((flags & TEXT0) != 0)
        {
          widgetText0 = new Label(subComposite,SWT.LEFT);
          widgetText0.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetText0 = null;
        }

        if ((flags & PROGRESS_BAR0) != 0)
        {
          widgetProgressBar0 = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBar0.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBar0.setMinimum(0.0);
          widgetProgressBar0.setMaximum(100.0);
        }
        else
        {
          widgetProgressBar0 = null;
        }

        if ((flags & TEXT1) != 0)
        {
          widgetText1 = new Label(subComposite,SWT.LEFT);
          widgetText1.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetText1 = null;
        }

        if ((flags & PROGRESS_BAR1) != 0)
        {
          widgetProgressBar1 = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBar1.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBar1.setMinimum(0.0);
          widgetProgressBar1.setMaximum(100.0);
        }
        else
        {
          widgetProgressBar1 = null;
        }

        if ((flags & LIST) != 0)
        {
          widgetText2 = new Label(subComposite,SWT.LEFT);
          widgetText2.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetList = new List(subComposite,SWT.BORDER|SWT.V_SCROLL);
          widgetList.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE)); row++;
        }
        else
        {
          widgetText2 = null;
          widgetList  = null;
        }
      }
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4,4));
    {
      widgetAbortCloseButton = new Button(composite,SWT.CENTER|SWT.BORDER);
      widgetAbortCloseButton.setText(BusyDialog.tr("Abort"));
      widgetAbortCloseButton.setEnabled((flags & ABORT_CLOSE) != 0);
      widgetAbortCloseButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,120,SWT.DEFAULT));
      widgetAbortCloseButton.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          if (isDone())
          {
            close();
          }
          else
          {
            abort();
          }
        }
      });
    }

    // resize handler
    dialog.addListener(SWT.Resize,new Listener()
    {
      public void handleEvent(Event event)
      {
        resizedFlag = true;
      }
    });

    // add escape key handler
    dialog.addTraverseListener(new TraverseListener()
    {
      public void keyTraversed(TraverseEvent traverseEvent)
      {
        Shell widget = (Shell)traverseEvent.widget;

        if (traverseEvent.detail == SWT.TRAVERSE_ESCAPE)
        {
          if (isDone())
          {
            close();
          }
          else
          {
            abort();
          }
          traverseEvent.doit = false;
        }
      }
    });

    // close handler to abort
    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        if ((flags & ABORT_CLOSE) != 0)
        {
          abort();
        }
        event.doit = false;
      }
    });

    // pack
    dialog.pack();

    // get location for dialog (keep 16 pixel away form right/bottom)
    Point cursorPoint = display.getCursorLocation();
    Rectangle displayBounds = display.getBounds();
    Rectangle bounds = dialog.getBounds();
    x = Math.min(Math.max(cursorPoint.x-bounds.width /2,0),displayBounds.width -bounds.width -16);
    y = Math.min(Math.max(cursorPoint.y-bounds.height/2,0),displayBounds.height-bounds.height-64);
    dialog.setLocation(x,y);

    // auto animate
    if ((flags & AUTO_ANIMATE) != 0)
    {
      Thread thread = new Thread()
      {
        public void run()
        {
          while (!animateQuitFlag && !dialog.isDisposed())
          {
            // animate
            display.syncExec(new Runnable()
            {
              public void run()
              {
                animate();
              }
            });

            // sleep
            try { Thread.sleep(animateInterval); } catch (InterruptedException exception) { /* ignore */ }
          }
        }
      };
      thread.start();
    }

    // run dialog
    doneFlag    = false;
    abortedFlag = false;
    resizedFlag = false;
    dialog.open();
    widgetAbortCloseButton.setFocus();
    display.update();
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param size width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   * @param maxListLength max. list length
   */
  public BusyDialog(Shell parentShell, String title, Point size, String message, int flags, int maxListLength)
  {
    this(parentShell,title,size.x,size.y,message,flags,maxListLength);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message, int flags)
  {
    this(parentShell,title,width,height,message,flags,-1);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param size width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, Point size, String message, int flags)
  {
    this(parentShell,title,size.x,size.y,message,flags);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message)
  {
    this(parentShell,title,width,height,message,ABORT_CLOSE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param size width/height of dialog
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, Point size, String message)
  {
    this(parentShell,title,size.x,size.y,message);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, int flags)
  {
    this(parentShell,title,width,height,null,flags);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param size width/height of dialog
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, Point size, int flags)
  {
    this(parentShell,title,size.x,size.y,flags);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   */
  public BusyDialog(Shell parentShell, String title, int width, int height)
  {
    this(parentShell,title,width,height,ABORT_CLOSE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param size width/height of dialog
   */
  public BusyDialog(Shell parentShell, String title, Point size)
  {
    this(parentShell,title,size.x,size.y);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   * @param flags busy dialog flags
   * @param maxListLength max. list length
   */
  public BusyDialog(Shell parentShell, String title, String message, int flags, int maxListLength)
  {
    this(parentShell,title,300,150,message,flags,maxListLength);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, String message, int flags)
  {
    this(parentShell,title,300,150,message,flags,100);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, String message)
  {
    this(parentShell,title,message,NONE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int flags)
  {
    this(parentShell,title,null,flags,100);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   */
  public BusyDialog(Shell parentShell, String title)
  {
    this(parentShell,title,ABORT_CLOSE);
  }

  /** add listener to dialog
   * @param eventType event type
   * @param listener listener to add
   */
  public void addListener(int eventType, Listener listener)
  {
    dialog.addListener(eventType,listener);
  }

  /** close busy dialog
   */
  public void close()
  {
    display.syncExec(new Runnable()
    {
      public void run()
      {
        if (!dialog.isDisposed())
        {
          dialog.dispose();
        }
      }
    });
  }

  /** check if dialog is closed
   * @return true iff closed
   */
  public boolean isClosed()
  {
    return dialog.isDisposed();
  }

  /** done busy dialog
   */
  public void done()
  {
    animateQuitFlag = true;
    doneFlag = true;
    display.syncExec(new Runnable()
    {
      public void run()
      {
        if (!widgetAbortCloseButton.isDisposed())
        {
          widgetAbortCloseButton.setText(BusyDialog.tr("Close"));
          widgetAbortCloseButton.setEnabled(true);
        }
      }
    });
  }

  /** check if "done"
   * @return true iff "done"
   */
  public boolean isDone()
  {
    return doneFlag;
  }

  /** abort and close busy dialog
   */
  public void abort()
  {
    abortedFlag = true;
    display.syncExec(new Runnable()
    {
      public void run()
      {
        if (!widgetAbortCloseButton.isDisposed())
        {
          widgetAbortCloseButton.setEnabled(false);
        }
      }
    });
  }

  /** check if "abort" button clicked
   * @return true iff "cancel" button clicked, false otherwise
   */
  public boolean isAborted()
  {
    return abortedFlag;
  }

  /** set message
   * @param format format string
   * @param args optional arguments
   */
  public void setMessage(final String format, final Object... args)
  {
    if ((widgetMessage != null) && !dialog.isDisposed())
    {
      display.asyncExec(new Runnable()
      {
        public void run()
        {
          String text = String.format(format,args);

          // set message text
          widgetMessage.setText(text);
          display.update();

          // resize dialog (it not manually changed)
          if (!resizedFlag)
          {
            GC gc = new GC(widgetMessage);
            int width = gc.stringExtent(text).x;
            gc.dispose();

            if (widgetMessage.getSize().x < width) dialog.pack();
          }
        }
      });
    }
  }

  /** set minimal progress value
   * @param i index 0|1
   * @param n value
   */
  public void setMinimum(int i, double n)
  {
    ProgressBar widgetProgressBar = null;
    switch (i)
    {
      case 0: widgetProgressBar = widgetProgressBar0; break;
      case 1: widgetProgressBar = widgetProgressBar1; break;
    }
    if (widgetProgressBar != null) widgetProgressBar.setMinimum(n);
  }

  /** set maximal progress value
   * @param i index 0|1
   * @param n value
   */
  public void setMaximum(int i, double n)
  {
    ProgressBar widgetProgressBar = null;
    switch (i)
    {
      case 0: widgetProgressBar = widgetProgressBar0; break;
      case 1: widgetProgressBar = widgetProgressBar1; break;
    }
    if (widgetProgressBar != null) widgetProgressBar.setMaximum(n);
  }

  /** set minimal progress value
   * @param n value
   */
  public void setMinimum(double n)
  {
    setMinimum(0,n);
  }

  /** set maximal progress value
   * @param n value
   */
  public void setMaximum(double n)
  {
    setMaximum(0,n);
  }

  /** update busy dialog text
   * @param i index 0|1
   * @param format format string
   * @param args optional arguments
   * @return true if closed, false otherwise
   */
  public boolean updateText(final int i, final String format, final Object... args)
  {
    if (!display.isDisposed() && !dialog.isDisposed())
    {
      display.asyncExec(new Runnable()
      {
        public void run()
        {
          animate();

          if (format != null)
          {
            String text = String.format(format,args);

            // set message text
            Label widgetText = null;
            switch (i)
            {
              case 0: widgetText = widgetText0; break;
              case 1: widgetText = widgetText1; break;
              case 2: widgetText = widgetText2; break;
            }
            if (   (widgetText != null)
                && !widgetText.isDisposed()
                && !text.equals(widgetText.getText())
               )
            {
              // set text
              widgetText.setText(text);

              // resize dialog (it not manually changed)
              if (!resizedFlag)
              {
                GC gc = new GC(widgetText);
                int width = gc.stringExtent(text).x;
                gc.dispose();

                if (widgetText.getSize().x < width) dialog.pack();
              }
            }
          }

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog text
   * @param i index 0|1
   * @param n number to show
   * @return true if closed, false otherwise
   */
  public boolean updateText(int i, Long n)
  {
    return updateText(i,"%d",n);
  }

  /** update busy dialog text
   * @param i index 0|1
   * @return true if closed, false otherwise
   */
  public boolean update(int i)
  {
    return updateText(i,(String)null);
  }

  /** update busy dialog text
   * @param text text to show (can be null)
   * @return true if closed, false otherwise
   */
  public boolean updateText(String format, final Object... args)
  {
    return updateText(0,format,args);
  }

  /** update busy dialog text
   * @param text text to show (can be null)
   * @return true if closed, false otherwise
   */
  public boolean updateText(String text)
  {
    return updateText(0,"%s",text);
  }

  /** update busy dialog progress bar
   * @param i index 0|1
   * @param n progress value
   * @return true if closed, false otherwise
   */
  public boolean updateProgressBar(final int i, final double n)
  {
    if (!dialog.isDisposed())
    {
      display.asyncExec(new Runnable()
      {
        public void run()
        {
          animate();

          // set progress bar value
          ProgressBar widgetProgressBar = null;
          switch (i)
          {
            case 0: widgetProgressBar = widgetProgressBar0; break;
            case 1: widgetProgressBar = widgetProgressBar1; break;
          }
          if (   (widgetProgressBar != null)
              && !widgetProgressBar.isDisposed()
              && (n != widgetProgressBar.getSelection())
             )
          {
            widgetProgressBar.setSelection(n);
          }

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog progress bar
   * @param n progress value
   * @return true if closed, false otherwise
   */
  public boolean updateProgressBar(double n)
  {
    return updateProgressBar(0,n);
  }

  /** update busy dialog
   * @param n number to show
   * @return true if closed, false otherwise
   */
  public boolean updateText(Long n)
  {
    return updateText(0,"%d",n);
  }

  /** update busy dialog
   * @return true if closed, false otherwise
   */
  public boolean update()
  {
    return update(0);
  }

  /** add to busy dialog list
   * @param format format string
   * @param args optional arguments
   * @return true if closed, false otherwise
   */
  public boolean updateList(final String format, final Object... args)
  {
    if (!dialog.isDisposed())
    {
      display.asyncExec(new Runnable()
      {
        public void run()
        {
          animate();

          if (format != null)
          {
            String text = String.format(format,args);

            // add list text
            if (widgetList == null) throw new InternalError("List not initialized");
            if (!widgetList.isDisposed())
            {
              widgetList.add(text);
              if (maxListLength >= 0)
              {
                while (widgetList.getItemCount() > maxListLength)
                {
                  widgetList.remove(0);
                }
              }
              widgetList.setSelection(widgetList.getItemCount()-1);
              widgetList.showSelection();


              // resize dialog (it not manually changed)
              if (!resizedFlag)
              {
                GC gc = new GC(widgetList);
                int width = gc.stringExtent(text).x;
                gc.dispose();

                if (widgetList.getSize().x < width) dialog.pack();
              }
            }
          }

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** add to busy dialog list
   * @param string string
   * @return true if update done, false otherwise
   */
  public boolean updateList(final String string)
  {
    return updateList("%s",string);
  }

  /** set animate interval
   * @param interval animate time interval [ms]
   */
  public void setAnimateInterval(int interval)
  {
    this.animateInterval = interval;
  }

  //-----------------------------------------------------------------------

  /** animate dialog
   */
  private void animate()
  {
    if (!dialog.isDisposed())
    {
      long timestamp = System.currentTimeMillis();
      if (timestamp > (animateTimestamp+animateInterval))
      {
        animateTimestamp  = timestamp;
        animateImageIndex = (animateImageIndex+1)%2;
        widgetImage.setImage(animateImages[animateImageIndex]);
      }
    }
  }
}

/* end of file */
