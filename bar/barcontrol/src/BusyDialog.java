/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BusyDialog.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: progress dialog
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
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
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

/** progress dialog
 */
class ProgressDialog
{
  private final Object[] result = new Object[1];
  private boolean        closedFlag;

  private Display        display;
  private Image          images[] = new Image[2];
  long                   imageTimestamp;
  int                    imageIndex;
  private Shell          dialog;
  private Label          widgetImage;
  private Label          widgetText;

  /** create progress dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   */
  public ProgressDialog(Shell parentShell, String title, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;
    int             x,y;

    display = parentShell.getDisplay();

    // load images
    images[0] = Widgets.loadImage(parentShell.getDisplay(),"busy0.gif");
    images[1] = Widgets.loadImage(parentShell.getDisplay(),"busy1.gif");
    imageTimestamp = System.currentTimeMillis();
    imageIndex = 0;

    // create dialog
    dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    dialog.setText(title);
    tableLayout = new TableLayout(new double[]{1,0},null,4);
    tableLayout.minWidth  = 300;
    tableLayout.minHeight = 70;
    dialog.setLayout(tableLayout);
    dialog.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message
    composite = new Composite(dialog,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      widgetImage = new Label(composite,SWT.LEFT);
      widgetImage.setImage(images[imageIndex]);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,10,10));

      widgetText = new Label(composite,SWT.LEFT|SWT.BORDER);
      widgetText.setText(message);
      widgetText.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4));
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Cancel");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.DEFAULT|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          close(true);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add escape key handler
    dialog.addTraverseListener(new TraverseListener()
    {
      public void keyTraversed(TraverseEvent traverseEvent)
      {
        Shell widget = (Shell)traverseEvent.widget;

        if (traverseEvent.detail == SWT.TRAVERSE_ESCAPE)
        {
//          widget.setData(false);
          close(false);
        }
      }
    });

    // close handler to get result
    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

//        result[0] = widget.getData();
        close(false);
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

    // run dialog
    closedFlag = false;
    dialog.open();
  }

  /** close progress dialog
   * @param result result
   */
  public void close(boolean result)
  {
    this.result[0] = result;
    closedFlag = true;
    dialog.dispose();
  }

  /** close progress dialog
   */
  public void close()
  {
    close(true);
  }

  /** check if "cancel" button clicked
   * @return true iff "cancel" button clicked, false otherwise
   */
  public boolean isAborted()
  {
    return closedFlag;
  }

  /** update progress dialog
   * @param text text to show
   */
  public boolean update(final String text)
  {
    display.update();
    display.readAndDispatch();

    if (!closedFlag)
    {
      long timestamp = System.currentTimeMillis();
      if (timestamp > (imageTimestamp+250))
      {
        imageTimestamp = timestamp;
        imageIndex     = (imageIndex+1)%3;
        widgetImage.setImage(images[imageIndex]);
      }
      widgetText.setText(text);

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update progress dialog
   * @param n number to show
   */
  public boolean update(Long n)
  {
    return update(Long.toString(n));
  }
}

/* end of file */
