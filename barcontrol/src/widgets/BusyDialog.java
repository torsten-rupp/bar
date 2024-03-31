/***********************************************************************\
*
* Contents: busy dialog
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.text.MessageFormat;

import java.util.concurrent.ConcurrentLinkedQueue;

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
public class BusyDialog
{
  // --------------------------- constants --------------------------------

  // busy dialog flags
  public final static int NONE               = 0;
  public final static int TEXT0              = 1 <<  0;   // show text line 0
  public final static int TEXT1              = 1 <<  1;   // show text line 1
  public final static int TEXT2              = 1 <<  2;   // show text line 2
  public final static int PROGRESS_BAR0      = 1 <<  3;   // show progress bar 0
  public final static int PROGRESS_BAR1      = 1 <<  4;   // show progress bar 1
  public final static int PROGRESS_BAR2      = 1 <<  5;   // show progress bar 2
  public final static int LIST               = 1 <<  6;   // show list
  public final static int ABORT_CLOSE        = 1 <<  7;   // abort/close button
  public final static int ENABLE_ABORT_CLOSE = 1 <<  8;   // enable abort/close button

  public final static int AUTO_ANIMATE       = 1 << 24;   // auto animate

  // --------------------------- variables --------------------------------
  private static I18n                         i18n;

  private Display                             display;

  private int                                 animateInterval = 100;  // [ms]
  private long                                animateTimestamp;
  private Image                               animateImages[] = new Image[2];
  private int                                 animateImageIndex;
  private boolean                             animateQuitFlag;

  private Shell                               dialog;
  private Label                               widgetImage;
  private Label                               widgetMessage;
  private Label                               widgetTexts[] = new Label[4];
  private ProgressBar                         widgetProgressBars[] = new ProgressBar[3];
  private List                                widgetList;
  private Button                              widgetAbortCloseButton;
  private int                                 maxListLength;

  private final String                        messageValue[] = new String[]{null};
  private final String                        textValues[] = new String[]{null,null,null,null};
  private final Double                        progressValues[] = new Double[]{0.0,0.0,0.0};
  private final ConcurrentLinkedQueue<String> listValues = new ConcurrentLinkedQueue<String>();

  private boolean                             doneFlag;
  private boolean                             abortedFlag;
  private boolean                             resizedFlag;
  private boolean                             listEmptyFlag;
  private Boolean                             redrawRequestedFlag;   // Note: avoid unlimited number of async-exec-request in event queue!

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
    tableLayout.maxWidth  = SWT.DEFAULT;
    tableLayout.minHeight = height;
    tableLayout.maxHeight = SWT.DEFAULT;
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

      double[] rowWeights = new double[1+2+2+2+2];
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
      if ((flags & TEXT2) != 0)
      {
        rowWeights[row] = ((flags & LIST) == 0) ? 1.0 : 0.0; row++;
      }
      if ((flags & PROGRESS_BAR2) != 0)
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
          widgetTexts[0] = new Label(subComposite,SWT.LEFT);
          widgetTexts[0].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetTexts[0] = null;
        }

        if ((flags & PROGRESS_BAR0) != 0)
        {
          widgetProgressBars[0] = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBars[0].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBars[0].setMinimum(0.0);
          widgetProgressBars[0].setMaximum(100.0);
        }
        else
        {
          widgetProgressBars[0] = null;
        }

        if ((flags & TEXT1) != 0)
        {
          widgetTexts[1] = new Label(subComposite,SWT.LEFT);
          widgetTexts[1].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetTexts[1] = null;
        }

        if ((flags & PROGRESS_BAR1) != 0)
        {
          widgetProgressBars[1] = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBars[1].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBars[1].setMinimum(0.0);
          widgetProgressBars[1].setMaximum(100.0);
        }
        else
        {
          widgetProgressBars[1] = null;
        }

        if ((flags & TEXT2) != 0)
        {
          widgetTexts[2] = new Label(subComposite,SWT.LEFT);
          widgetTexts[2].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetTexts[2] = null;
        }

        if ((flags & PROGRESS_BAR2) != 0)
        {
          widgetProgressBars[2] = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBars[2].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBars[2].setMinimum(0.0);
          widgetProgressBars[2].setMaximum(100.0);
        }
        else
        {
          widgetProgressBars[2] = null;
        }

        if ((flags & LIST) != 0)
        {
          widgetTexts[3] = new Label(subComposite,SWT.LEFT);
          widgetTexts[3].setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetList = new List(subComposite,SWT.BORDER|SWT.V_SCROLL);
          widgetList.setBackground(display.getSystemColor(SWT.COLOR_WIDGET_BACKGROUND));
          widgetList.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE)); row++;
        }
        else
        {
          widgetTexts[3] = null;
          widgetList     = null;
        }
      }
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4,4));
    {
      if ((flags & ABORT_CLOSE) != 0)
      {
        widgetAbortCloseButton = new Button(composite,SWT.CENTER|SWT.BORDER);
        widgetAbortCloseButton.setText(BusyDialog.tr("Abort"));
        widgetAbortCloseButton.setEnabled((flags & ENABLE_ABORT_CLOSE) != 0);
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
      else
      {
        widgetAbortCloseButton = null;
      }
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

        if ((flags & ENABLE_ABORT_CLOSE) != 0)
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
    doneFlag            = false;
    abortedFlag         = false;
    resizedFlag         = (width != SWT.DEFAULT) || (height != SWT.DEFAULT);
    listEmptyFlag       = true;
    redrawRequestedFlag = false;
    dialog.open();
    if (widgetAbortCloseButton != null) widgetAbortCloseButton.setFocus();
    display.update();
    while (display.readAndDispatch())
    {
      // nothing to do
    }
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
    this(parentShell,title,width,height,message,ABORT_CLOSE|ENABLE_ABORT_CLOSE);
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
    this(parentShell,title,width,height,ABORT_CLOSE|ENABLE_ABORT_CLOSE);
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
    this(parentShell,title,600,400,message,flags,maxListLength);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, String message, int flags)
  {
    this(parentShell,title,600,400,message,flags,100);
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
    this(parentShell,title,ABORT_CLOSE|ENABLE_ABORT_CLOSE);
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
        if ((widgetAbortCloseButton != null) && !widgetAbortCloseButton.isDisposed())
        {
          // set progress bars to 100%
          if (widgetProgressBars[0] != null) widgetProgressBars[0].setSelection(widgetProgressBars[0].getMaximum());
          if (widgetProgressBars[1] != null) widgetProgressBars[1].setSelection(widgetProgressBars[1].getMaximum());
          if (widgetProgressBars[2] != null) widgetProgressBars[2].setSelection(widgetProgressBars[2].getMaximum());

          // change button text
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
        if ((widgetAbortCloseButton != null) && !widgetAbortCloseButton.isDisposed())
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
   * @return true if closed, false otherwise
   */
  public boolean setMessage(final String format, final Object... args)
  {
    if (!display.isDisposed() && !dialog.isDisposed())
    {
      messageValue[0] = String.format(format,args);
      updateValues();

      return true;
    }
    else
    {
      return false;
    }
  }

  /** set minimal progress value
   * @param i index 0..2
   * @param n value
   */
  public void setMinimum(int i, double n)
  {
    if (widgetProgressBars[i] != null) widgetProgressBars[i].setMinimum(n);
  }

  /** set maximal progress value
   * @param i index 0..2
   * @param n value
   */
  public void setMaximum(int i, double n)
  {
    if (widgetProgressBars[i] != null) widgetProgressBars[i].setMaximum(n);
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
   * @param i index 0..3
   * @param format format string
   * @param args optional arguments
   * @return true if closed, false otherwise
   */
  public boolean updateText(final int i, String format, Object... args)
  {
    if (!display.isDisposed() && !dialog.isDisposed())
    {
      textValues[i] = (format != null) ? String.format(format,args) : null;
      updateValues();

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog text
   * @param i index 0..3
   * @param n number to show
   * @return true if closed, false otherwise
   */
  public boolean updateText(int i, Long n)
  {
    return updateText(i,"%d",n);
  }

  /** update busy dialog text
   * @param i index 0..3
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
  public boolean updateText(String format, Object... args)
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
   * @param i index 0..2
   * @param n progress value
   * @return true if closed, false otherwise
   */
  public boolean updateProgressBar(int i, double value)
  {
    if (!display.isDisposed() && !dialog.isDisposed())
    {
      progressValues[i] = value;
      updateValues();

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
    if (!display.isDisposed() && !dialog.isDisposed())
    {
      if (format != null)
      {
        listValues.add(String.format(format,args));
        updateValues();
      }

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
  public boolean updateList(String string)
  {
    return updateList("%s",string);
  }

  /** check if dialog list is empty
   * @return true iff dialog list is empty
   */
  public boolean isListEmpty()
  {
    return listEmptyFlag;
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

  /** update values
   */
  private void updateValues()
  {
    if (!redrawRequestedFlag)
    {
      redrawRequestedFlag = true;

      display.asyncExec(new Runnable()
      {
        public void run()
        {
          int    i;
          String text;

          if (!dialog.isDisposed())
          {
            animate();

            if (messageValue[0] != null)
            {
              // set text
              widgetMessage.setText(messageValue[0]);

              // resize dialog (it not manually changed)
              if (!resizedFlag)
              {
                GC gc = new GC(widgetMessage);
                int width = gc.stringExtent(messageValue[0]).x;
                gc.dispose();

                if (widgetMessage.getSize().x < width) dialog.pack();
              }

              messageValue[0] = null;
            }

            for (i = 0; i < 4; i++)
            {
              if (textValues[i] != null)
              {
                // set text
                if (   (widgetTexts[i] != null)
                    && !widgetTexts[i].isDisposed()
                    && !textValues[i].equals(widgetTexts[i].getText())
                   )
                {
                  // set text
                  widgetTexts[i].setText(textValues[i]);

                  // resize dialog (it not manually changed)
                  if (!resizedFlag)
                  {
                    GC gc = new GC(widgetTexts[i]);
                    int width = gc.stringExtent(textValues[i]).x;
                    gc.dispose();

                    if (widgetTexts[i].getSize().x < width) dialog.pack();
                  }
                }

                textValues[i] = null;
              }
            }

            for (i = 0; i < 3; i++)
            {
              if (progressValues[i] != null)
              {
                // set progress bar value
                if (   (widgetProgressBars[i] != null)
                    && !widgetProgressBars[i].isDisposed()
                   )
                {
                  widgetProgressBars[i].setSelection(progressValues[i]);
                }

                progressValues[i] = null;
              }
            }

            while ((text = listValues.poll()) != null)
            {
              // set list
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
              listEmptyFlag = false;

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

          redrawRequestedFlag = false;
        }
      });
    }
  }
}

/* end of file */
