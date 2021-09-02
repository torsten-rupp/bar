/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: Server settings
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;

import org.eclipse.swt.custom.StyledText;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/**
 * Edit BAR server settings
 */
public class ServerSettings
{
  /** storage maintenance data
   */
  static class MaintenanceData implements Cloneable
  {
    final static int NONE = 0;
    final static int ANY  = -1;
    final static int MON  = 0;
    final static int TUE  = 1;
    final static int WED  = 2;
    final static int THU  = 3;
    final static int FRI  = 4;
    final static int SAT  = 5;
    final static int SUN  = 6;

    int id;
    int year,month,day;
    int weekDays;
    int beginHour,beginMinute;
    int endHour,endMinute;

    /** create storage maintenance data
     * @param id id
     * @param year year
     * @param month month
     * @param day day
     * @param weekDays week days
     * @param beginHour begin hour
     * @param beginMinute begin minute
     * @param endHour end hour
     * @param endMinute end minute
     */
    MaintenanceData(int id, int year, int month, int day, int weekDays, int beginHour, int beginMinute, int endHour, int endMinute)
    {
      assert (year == ANY) || (year >= 1) : year;
      assert (month == ANY) || ((month >= 1) && (month <= 12)) : month;
      assert (day == ANY) || ((day >= 1) && (day <= 31)) : day;
      assert    (weekDays == ANY)
             || ((weekDays & ~(  (1 << MaintenanceData.MON)
                               | (1 << MaintenanceData.TUE)
                               | (1 << MaintenanceData.WED)
                               | (1 << MaintenanceData.THU)
                               | (1 << MaintenanceData.FRI)
                               | (1 << MaintenanceData.SAT)
                               | (1 << MaintenanceData.SUN)
                              )) == 0
                ) : weekDays;
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;
      assert (beginMinute == ANY) || ((beginMinute >= 0) && (beginMinute <= 59)) : beginMinute;
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      this.id          = id;
      this.year        = year;
      this.month       = month;
      this.day         = day;
      this.weekDays    = weekDays;
      this.beginHour   = beginHour;
      this.beginMinute = beginMinute;
      this.endHour     = endHour;
      this.endMinute   = endMinute;
    }

    /** create storage maintenance data
     * @param id id
     */
    MaintenanceData(int id)
    {
      this(id,ANY,ANY,ANY,ANY,ANY,ANY,ANY,ANY);
    }

    /** create storage maintenance data
     * @param id id
     * @param date date
     * @param weekDays week days
     * @param begin begin time
     * @param end end time
     */
    MaintenanceData(int id, String date, String weekDays, String begin, String end)
    {
      this(id);
      setDate(date);
      setWeekDays(weekDays);
      setBeginTime(begin);
      setEndTime(end);
    }

    /** create storage maintenance data
     */
    MaintenanceData()
    {
      this(0);
    }

    /** clone storage maintenance data object
     * @return clone of object
     */
    public MaintenanceData clone()
    {
      return new MaintenanceData(id,year,month,day,weekDays,beginHour,beginMinute,endHour,endMinute);
    }

    /** get year value
     * @return year string
     */
    String getYear()
    {
      assert (year == ANY) || (year >= 1) : year;

      return (year != ANY) ? String.format("%04d",year) : "*";
    }

    /** get month value
     * @return month string
     */
    String getMonth()
    {
      assert (month == ANY) || ((month >= 1) && (month <= 12)) : month;

      switch (month)
      {
        case ANY: return "*";
        case 1:   return "Jan";
        case 2:   return "Feb";
        case 3:   return "Mar";
        case 4:   return "Apr";
        case 5:   return "May";
        case 6:   return "Jun";
        case 7:   return "Jul";
        case 8:   return "Aug";
        case 9:   return "Sep";
        case 10:  return "Oct";
        case 11:  return "Nov";
        case 12:  return "Dec";
        default:  return "*";
      }
    }

    /** get day value
     * @return day string
     */
    String getDay()
    {
      assert (day == ANY) || ((day >= 1) && (day <= 31)) : day;

      return (day != ANY) ? String.format("%02d",day) : "*";
    }

    /** get date value
     * @return date string
     */
    String getDate()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getYear());
      buffer.append('-');
      buffer.append(getMonth());
      buffer.append('-');
      buffer.append(getDay());

      return buffer.toString();
    }

    /** get week days value
     * @return week days string
     */
    String getWeekDays()
    {
      assert    (weekDays == ANY)
             || ((weekDays & ~(  (1 << MaintenanceData.MON)
                               | (1 << MaintenanceData.TUE)
                               | (1 << MaintenanceData.WED)
                               | (1 << MaintenanceData.THU)
                               | (1 << MaintenanceData.FRI)
                               | (1 << MaintenanceData.SAT)
                               | (1 << MaintenanceData.SUN)
                              )) == 0
                ) : weekDays;

      if (weekDays == ANY)
      {
        return "*";
      }
      else
      {
        StringBuilder buffer = new StringBuilder();

        if ((weekDays & (1 << MaintenanceData.MON)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Mon")); }
        if ((weekDays & (1 << MaintenanceData.TUE)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Tue")); }
        if ((weekDays & (1 << MaintenanceData.WED)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Wed")); }
        if ((weekDays & (1 << MaintenanceData.THU)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Thu")); }
        if ((weekDays & (1 << MaintenanceData.FRI)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Fri")); }
        if ((weekDays & (1 << MaintenanceData.SAT)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sat")); }
        if ((weekDays & (1 << MaintenanceData.SUN)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sun")); }

        return buffer.toString();
      }
    }

    /** get begin hour value
     * @return begin hour string
     */
    String getBeginHour()
    {
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;

      return (beginHour != ANY) ? String.format("%02d",beginHour) : "*";
    }

    /** get begin minute value
     * @return begin minute string
     */
    String getBeginMinute()
    {
      assert (beginMinute == ANY) || ((beginMinute >= 0) && (beginMinute <= 59)) : beginMinute;

      return (beginMinute != ANY) ? String.format("%02d",beginMinute) : "*";
    }

    /** get begin time value
     * @return time string
     */
    String getBeginTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getBeginHour());
      buffer.append(':');
      buffer.append(getBeginMinute());

      return buffer.toString();
    }

    /** get end hour value
     * @return end hour string
     */
    String getEndHour()
    {
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;

      return (endHour != ANY) ? String.format("%02d",endHour) : "*";
    }

    /** get end minute value
     * @return end minute string
     */
    String getEndMinute()
    {
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      return (endMinute != ANY) ? String.format("%02d",endMinute) : "*";
    }

    /** get end time value
     * @return time string
     */
    String getEndTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getEndHour());
      buffer.append(':');
      buffer.append(getEndMinute());

      return buffer.toString();
    }

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     */
    private void setDate(String year, String month, String day)
    {
      this.year = !year.equals("*") ? Integer.parseInt(year) : ANY;
      if      (month.equals("*")) this.month = ANY;
      else if (month.toLowerCase().equals("jan")) this.month =  1;
      else if (month.toLowerCase().equals("feb")) this.month =  2;
      else if (month.toLowerCase().equals("mar")) this.month =  3;
      else if (month.toLowerCase().equals("apr")) this.month =  4;
      else if (month.toLowerCase().equals("may")) this.month =  5;
      else if (month.toLowerCase().equals("jun")) this.month =  6;
      else if (month.toLowerCase().equals("jul")) this.month =  7;
      else if (month.toLowerCase().equals("aug")) this.month =  8;
      else if (month.toLowerCase().equals("sep")) this.month =  9;
      else if (month.toLowerCase().equals("oct")) this.month = 10;
      else if (month.toLowerCase().equals("nov")) this.month = 11;
      else if (month.toLowerCase().equals("dec")) this.month = 12;
      else
      {
        try
        {
          this.month = Integer.parseInt(month);
        }
        catch (NumberFormatException exception)
        {
          this.month = ANY;
        }
      }
      this.day = !day.equals("*") ? Integer.parseInt(day) : ANY;
    }

    /** set date
     * @param date date string
     */
    private void setDate(String date)
    {
      String[] parts = date.split("-");
      setDate(parts[0],parts[1],parts[2]);
    }

    /** set week days
     * @param weekDays week days string; values separated by ','
     */
    void setWeekDays(String weekDays)
    {
      if (weekDays.equals("*"))
      {
        this.weekDays = MaintenanceData.ANY;
      }
      else
      {
        this.weekDays = MaintenanceData.NONE;
        for (String name : weekDays.split(","))
        {
          if      (name.toLowerCase().equals("mon")) this.weekDays |= (1 << MaintenanceData.MON);
          else if (name.toLowerCase().equals("tue")) this.weekDays |= (1 << MaintenanceData.TUE);
          else if (name.toLowerCase().equals("wed")) this.weekDays |= (1 << MaintenanceData.WED);
          else if (name.toLowerCase().equals("thu")) this.weekDays |= (1 << MaintenanceData.THU);
          else if (name.toLowerCase().equals("fri")) this.weekDays |= (1 << MaintenanceData.FRI);
          else if (name.toLowerCase().equals("sat")) this.weekDays |= (1 << MaintenanceData.SAT);
          else if (name.toLowerCase().equals("sun")) this.weekDays |= (1 << MaintenanceData.SUN);
        }
      }
    }

    /** set week days
     * @param monFlag true for Monday
     * @param tueFlag true for Tuesday
     * @param wedFlag true for Wednesday
     * @param thuFlag true for Thursday
     * @param friFlag true for Friday
     * @param satFlag true for Saturday
     * @param SunFlag true for Sunday
     */
    void setWeekDays(boolean monFlag,
                     boolean tueFlag,
                     boolean wedFlag,
                     boolean thuFlag,
                     boolean friFlag,
                     boolean satFlag,
                     boolean SunFlag
                    )
    {

      if (   monFlag
          && tueFlag
          && wedFlag
          && thuFlag
          && friFlag
          && satFlag
          && SunFlag
         )
      {
        this.weekDays = MaintenanceData.ANY;
      }
      else
      {
        this.weekDays = 0;
        if (monFlag) this.weekDays |= (1 << MaintenanceData.MON);
        if (tueFlag) this.weekDays |= (1 << MaintenanceData.TUE);
        if (wedFlag) this.weekDays |= (1 << MaintenanceData.WED);
        if (thuFlag) this.weekDays |= (1 << MaintenanceData.THU);
        if (friFlag) this.weekDays |= (1 << MaintenanceData.FRI);
        if (satFlag) this.weekDays |= (1 << MaintenanceData.SAT);
        if (SunFlag) this.weekDays |= (1 << MaintenanceData.SUN);
      }
    }

    /** set time
     * @param hour hour value
     * @param minute minute value
     */
    void setBeginTime(String hour, String minute)
    {
      this.beginHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.beginMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
    }

    /** set begin time
     * @param time time string
     */
    void setBeginTime(String time)
    {
      String[] parts = time.split(":");
      setBeginTime(parts[0],parts[1]);
    }

    /** set end time
     * @param hour hour value
     * @param minute minute value
     */
    void setEndTime(String hour, String minute)
    {
      this.endHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.endMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
    }

    /** set end time
     * @param time time string
     */
    void setEndTime(String time)
    {
      String[] parts = time.split(":");
      setEndTime(parts[0],parts[1]);
    }

    /** check if week day enabled
     * @param weekDay week data
     * @return TRUE iff enabled
     */
    boolean weekDayIsEnabled(int weekDay)
    {
      assert(   (weekDay == MaintenanceData.MON)
             || (weekDay == MaintenanceData.TUE)
             || (weekDay == MaintenanceData.WED)
             || (weekDay == MaintenanceData.THU)
             || (weekDay == MaintenanceData.FRI)
             || (weekDay == MaintenanceData.SAT)
             || (weekDay == MaintenanceData.SUN)
            );

      return (weekDays == MaintenanceData.ANY) || ((weekDays & (1 << weekDay)) != 0);
    }

    /** convert week days to string
     * @return week days string
     */
    String weekDaysToString()
    {
      assert    (weekDays == ANY)
             || ((weekDays & ~(  (1 << MaintenanceData.MON)
                               | (1 << MaintenanceData.TUE)
                               | (1 << MaintenanceData.WED)
                               | (1 << MaintenanceData.THU)
                               | (1 << MaintenanceData.FRI)
                               | (1 << MaintenanceData.SAT)
                               | (1 << MaintenanceData.SUN)
                              )) == 0
                ) : weekDays;

      if (weekDays == ANY)
      {
        return "*";
      }
      else
      {
        StringBuilder buffer = new StringBuilder();

        if ((weekDays & (1 << MaintenanceData.MON)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Mon"); }
        if ((weekDays & (1 << MaintenanceData.TUE)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Tue"); }
        if ((weekDays & (1 << MaintenanceData.WED)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Wed"); }
        if ((weekDays & (1 << MaintenanceData.THU)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Thu"); }
        if ((weekDays & (1 << MaintenanceData.FRI)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Fri"); }
        if ((weekDays & (1 << MaintenanceData.SAT)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Sat"); }
        if ((weekDays & (1 << MaintenanceData.SUN)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Sun"); }

        return buffer.toString();
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Maintenance {"+id+", "+year+", "+month+", "+day+", "+beginHour+", "+beginMinute+", "+endHour+", "+endMinute+"}";
    }
  }

  /** maintenance data comparator
   */
  static class MaintenanceDataComparator implements Comparator<MaintenanceData>
  {
    // sort modes
    enum SortModes
    {
      DATE,
      WEEKDAYS,
      BEGIN_TIME,
      END_TIME
    };

    private SortModes sortMode;

//    private final String[] WEEK_DAYS = new String[]{"mon","tue","wed","thu","fri","sat","sun"};

    /** create maintenance data comparator
     * @param table maintenance table
     * @param sortColumn column to sort
     */
    MaintenanceDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.WEEKDAYS;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.BEGIN_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.END_TIME;
      else                                       sortMode = SortModes.DATE;
    }

    /** create maintenance data comparator
     * @param table maintenance table
     */
    MaintenanceDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.WEEKDAYS;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.BEGIN_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.END_TIME;
      else                                       sortMode = SortModes.DATE;
    }

    /** compare maintenance data
     * @param maintenanceData1, maintenanceData2 index maintenance data to compare
     * @return -1 iff maintenanceData1 < maintenanceData2,
                0 iff maintenanceData1 = maintenanceData2,
                1 iff maintenanceData1 > maintenanceData2
     */
    public int compare(MaintenanceData maintenanceData1, MaintenanceData maintenanceData2)
    {
      switch (sortMode)
      {
        case DATE:
          String date1 = maintenanceData1.getDate();
          String date2 = maintenanceData2.getDate();

          return date1.compareTo(date2);
        case WEEKDAYS:
          if      (maintenanceData1.weekDays < maintenanceData2.weekDays) return -1;
          else if (maintenanceData1.weekDays > maintenanceData2.weekDays) return  1;
          else                                                            return  0;
        case BEGIN_TIME:
          String beginTime1 = maintenanceData1.getBeginTime();
          String beginTime2 = maintenanceData2.getBeginTime();

          return beginTime1.compareTo(beginTime2);
        case END_TIME:
          String endTime1 = maintenanceData1.getEndTime();
          String endTime2 = maintenanceData2.getEndTime();

          return endTime1.compareTo(endTime2);
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "MaintenanceDataComparator {"+sortMode+"}";
    }
  }

  /**
   * Storage server types
   */
  enum ServerTypes
  {
    NONE,

    FILE,
    FTP,
    SSH,
    WEBDAV;

    /** parse type string
     * @param string type string
     * @return priority
     */
    static ServerTypes parse(String string)
    {
      ServerTypes type;

      if      (string.equalsIgnoreCase("FILE"))
      {
        type = ServerTypes.FILE;
      }
      else if (string.equalsIgnoreCase("FTP"))
      {
        type = ServerTypes.FTP;
      }
      else if (string.equalsIgnoreCase("SSH"))
      {
        type = ServerTypes.SSH;
      }
      else if (string.equalsIgnoreCase("WEBDAV"))
      {
        type = ServerTypes.WEBDAV;
      }
      else
      {
        type = ServerTypes.NONE;
      }

      return type;
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case FILE:   return "file";
        case FTP:    return "ftp";
        case SSH:    return "ssh";
        case WEBDAV: return "webdav";
        default:     return "";
      }
    }
  };

  /** storage server data
   */
  static class ServerData implements Cloneable
  {
    int         id;
    String      name;
    ServerTypes type;
    String      loginName;
    int         port;
    String      password;
    String      publicKey;
    String      privateKey;
    int         maxConnectionCount;
    long        maxStorageSize;

    /** create storage server data
     * @param id id
     * @param name name
     * @param type server type
     * @param loginName login name
     * @param port port number
     * @param maxConnectionCount max. concurrent connections
     * @param maxStorageSize max. storage size [bytes]
     */
    ServerData(int id, String name, ServerTypes type, String loginName, int port, int maxConnectionCount, long maxStorageSize)
    {
      this.id                 = id;
      this.name               = name;
      this.type               = type;
      this.port               = port;
      this.loginName          = loginName;
      this.publicKey          = null;
      this.privateKey         = null;
      this.maxConnectionCount = maxConnectionCount;
      this.maxStorageSize     = maxStorageSize;
    }

    /** create storage server data
     * @param id id
     * @param name name
     * @param type server type
     */
    ServerData(int id, String name, ServerTypes type)
    {
      this(id,name,type,"",0,0,0L);
    }

    /** create storage server data
     */
    ServerData()
    {
      this(0,"",ServerTypes.NONE);
    }

    /** clone storage server data object
     * @return clone of object
     */
    public ServerData clone()
    {
      return new ServerData(id,name,type,loginName,port,maxConnectionCount,maxStorageSize);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Server {"+id+", "+name+", "+type.toString()+"}";
    }
  };

  /** server data comparator
   */
  static class ServerDataComparator implements Comparator<ServerData>
  {
    // sort modes
    enum SortModes
    {
      TYPE,
      NAME
    };

    private SortModes sortMode;

    /** create server data comparator
     * @param table server table
     * @param sortColumn column to sort
     */
    ServerDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.TYPE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.NAME;
      else                                       sortMode = SortModes.TYPE;
    }

    /** create server data comparator
     * @param table server table
     */
    ServerDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare server data
     * @param serverData1, serverData2 server data to compare
     * @return -1 iff serverData1 < serverData2,
                0 iff serverData1 = serverData2,
                1 iff serverData1 > serverData2
     */
    public int compare(ServerData serverData1, ServerData serverData2)
    {
      switch (sortMode)
      {
        case TYPE:
          if      (serverData1.type.toString().compareTo(serverData2.type.toString()) < 0)
          {
            return -1;
          }
          else if (serverData1.type.toString().compareTo(serverData2.type.toString()) > 0)
          {
            return 1;
          }
          else
          {
            return serverData1.name.compareTo(serverData2.name);
          }
        case NAME:
          return serverData1.name.compareTo(serverData2.name);
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "ServerDataComparator {"+sortMode+"}";
    }
  }

  private static Display display;

  // colors
  private final static Color COLOR_INACTIVE = new Color(null,0xF4,0xF4,0xF4);

  // images
  private static Image IMAGE_TOGGLE_MARK;

  // last key file name
  private static String lastKeyFileName = "";

  /** edit server settings
   * @param shell shell
   */
  public static void serverSettings(Shell shell)
  {
    TabFolder   tabFolder,subTabFolder;
    Composite   tab;
    Composite   composite,subComposite,subSubComposite;
    Label       label;
    Text        text;
    Combo       combo;
    Spinner     spinner;
    TableColumn tableColumn;
    Button      button;
    int         row;

    WidgetVariable          tmpDirectory               = new WidgetVariable<String >("tmp-directory",                 ""   );
    WidgetVariable          maxTmpSize                 = new WidgetVariable<String >("max-tmp-size",                  ""   );
    WidgetVariable          niceLevel                  = new WidgetVariable<Integer>("nice-level",                    0    );
    WidgetVariable          maxThreads                 = new WidgetVariable<Integer>("max-threads",                   0    );
//    WidgetVariable          maxBandWidth               = new WidgetVariable<String >("max-band-width",              ""   );
    WidgetVariable          compressMinSize            = new WidgetVariable<String >("compress-min-size",             ""   );
    WidgetVariable          serverJobsDirectory        = new WidgetVariable<String >("jobs-directory",                ""   );

    WidgetVariable          continuousMaxSize          = new WidgetVariable<String >("continuous-max-size",           ""   );

    WidgetVariable          indexDatabase              = new WidgetVariable<String >("index-database",                ""   );
    WidgetVariable          indexDatabaseUpdate        = new WidgetVariable<Boolean>("index-database-update",         false);
    WidgetVariable          indexDatabaseAutoUpdate    = new WidgetVariable<Boolean>("index-database-auto-update",    false);
//    WidgetVariable          indexDatabaseMaxBandWidth  = new WidgetVariable<String>("index-database-max-band-width",""   );
    WidgetVariable          indexDatabaseKeepTime      = new WidgetVariable<String >("index-database-keep-time",      ""   );

    WidgetVariable          cdDevice                   = new WidgetVariable<String >("cd-device",                     ""   );
    WidgetVariable          cdRequestVolumeCommand     = new WidgetVariable<String >("cd-request-volume-command",     ""   );
    WidgetVariable          cdUnloadCommand            = new WidgetVariable<String >("cd-unload-volume-command",      ""   );
    WidgetVariable          cdLoadCommand              = new WidgetVariable<String >("cd-load-volume-command",        ""   );
    WidgetVariable          cdVolumeSize               = new WidgetVariable<String >("cd-volume-size",                ""   );
    WidgetVariable          cdImagePreCommand          = new WidgetVariable<String >("cd-image-pre-command",          ""   );
    WidgetVariable          cdImagePostCommand         = new WidgetVariable<String >("cd-image-post-command",         ""   );
    WidgetVariable          cdImageCommandCommand      = new WidgetVariable<String >("cd-image-command",              ""   );
    WidgetVariable          cdECCPreCommand            = new WidgetVariable<String >("cd-ecc-pre-command",            ""   );
    WidgetVariable          cdECCPostCommand           = new WidgetVariable<String >("cd-ecc-post-command",           ""   );
    WidgetVariable          cdECCCommand               = new WidgetVariable<String >("cd-ecc-command",                ""   );
    WidgetVariable          cdBlankCommand             = new WidgetVariable<String >("cd-blank-command",              ""   );
    WidgetVariable          cdWritePreCommand          = new WidgetVariable<String >("cd-write-pre-command",          ""   );
    WidgetVariable          cdWritePostCommand         = new WidgetVariable<String >("cd-write-post-command",         ""   );
    WidgetVariable          cdWriteCommand             = new WidgetVariable<String >("cd-write-command",              ""   );
    WidgetVariable          cdWriteImageCommand        = new WidgetVariable<String >("cd-write-image-command",        ""   );

    WidgetVariable          dvdDevice                  = new WidgetVariable<String >("dvd-device",                    ""   );
    WidgetVariable          dvdRequestVolumeCommand    = new WidgetVariable<String >("dvd-request-volume-command",    ""   );
    WidgetVariable          dvdUnloadCommand           = new WidgetVariable<String >("dvd-unload-volume-command",     ""   );
    WidgetVariable          dvdLoadCommand             = new WidgetVariable<String >("dvd-load-volume-command",       ""   );
    WidgetVariable          dvdVolumeSize              = new WidgetVariable<String >("dvd-volume-size",               ""   );
    WidgetVariable          dvdImagePreCommand         = new WidgetVariable<String >("dvd-image-pre-command",         ""   );
    WidgetVariable          dvdImagePostCommand        = new WidgetVariable<String >("dvd-image-post-command",        ""   );
    WidgetVariable          dvdImageCommandCommand     = new WidgetVariable<String >("dvd-image-command",             ""   );
    WidgetVariable          dvdECCPreCommand           = new WidgetVariable<String >("dvd-ecc-pre-command",           ""   );
    WidgetVariable          dvdECCPostCommand          = new WidgetVariable<String >("dvd-ecc-post-command",          ""   );
    WidgetVariable          dvdECCCommand              = new WidgetVariable<String >("dvd-ecc-command",               ""   );
    WidgetVariable          dvdBlankCommand            = new WidgetVariable<String >("dvd-blank-command",             ""   );
    WidgetVariable          dvdWritePreCommand         = new WidgetVariable<String >("dvd-write-pre-command",         ""   );
    WidgetVariable          dvdWritePostCommand        = new WidgetVariable<String >("dvd-write-post-command",        ""   );
    WidgetVariable          dvdWriteCommand            = new WidgetVariable<String >("dvd-write-command",             ""   );
    WidgetVariable          dvdWriteImageCommand       = new WidgetVariable<String >("dvd-write-image-command",       ""   );

    WidgetVariable          bdDevice                   = new WidgetVariable<String >("bd-device",                     ""   );
    WidgetVariable          bdRequestVolumeCommand     = new WidgetVariable<String >("bd-request-volume-command",     ""   );
    WidgetVariable          bdUnloadCommand            = new WidgetVariable<String >("bd-unload-volume-command",      ""   );
    WidgetVariable          bdLoadCommand              = new WidgetVariable<String >("bd-load-volume-command",        ""   );
    WidgetVariable          bdVolumeSize               = new WidgetVariable<String >("bd-volume-size",                ""   );
    WidgetVariable          bdImagePreCommand          = new WidgetVariable<String >("bd-image-pre-command",          ""   );
    WidgetVariable          bdImagePostCommand         = new WidgetVariable<String >("bd-image-post-command",         ""   );
    WidgetVariable          bdImageCommandCommand      = new WidgetVariable<String >("bd-image-command",              ""   );
    WidgetVariable          bdECCPreCommand            = new WidgetVariable<String >("bd-ecc-pre-command",            ""   );
    WidgetVariable          bdECCPostCommand           = new WidgetVariable<String >("bd-ecc-post-command",           ""   );
    WidgetVariable          bdECCCommand               = new WidgetVariable<String >("bd-ecc-command",                ""   );
    WidgetVariable          bdBlankCommand             = new WidgetVariable<String >("bd-blank-command",              ""   );
    WidgetVariable          bdWritePreCommand          = new WidgetVariable<String >("bd-write-pre-command",          ""   );
    WidgetVariable          bdWritePostCommand         = new WidgetVariable<String >("bd-write-post-command",         ""   );
    WidgetVariable          bdWriteCommand             = new WidgetVariable<String >("bd-write-command",              ""   );
    WidgetVariable          bdWriteImageCommand        = new WidgetVariable<String >("bd-write-image-command",        ""   );

    WidgetVariable          deviceName                 = new WidgetVariable<String >("device",                        ""   );
    WidgetVariable          deviceRequestVolumeCommand = new WidgetVariable<String >("device-request-volume-command", ""   );
    WidgetVariable          deviceUnloadCommand        = new WidgetVariable<String >("device-unload-volume-command",  ""   );
    WidgetVariable          deviceLoadCommand          = new WidgetVariable<String >("device-load-volume-command",    ""   );
    WidgetVariable          deviceVolumeSize           = new WidgetVariable<String >("device-volume-size",            ""   );
    WidgetVariable          deviceImagePreCommand      = new WidgetVariable<String >("device-image-pre-command",      ""   );
    WidgetVariable          deviceImagePostCommand     = new WidgetVariable<String >("device-image-post-command",     ""   );
    WidgetVariable          deviceImageCommandCommand  = new WidgetVariable<String >("device-image-command",          ""   );
    WidgetVariable          deviceECCPreCommand        = new WidgetVariable<String >("device-ecc-pre-command",        ""   );
    WidgetVariable          deviceECCPostCommand       = new WidgetVariable<String >("device-ecc-post-command",       ""   );
    WidgetVariable          deviceECCCommand           = new WidgetVariable<String >("device-ecc-command",            ""   );
    WidgetVariable          deviceBlankCommand         = new WidgetVariable<String >("device-blank-command",          ""   );
    WidgetVariable          deviceWritePreCommand      = new WidgetVariable<String >("device-write-pre-command",      ""   );
    WidgetVariable          deviceWritePostCommand     = new WidgetVariable<String >("device-write-post-command",     ""   );
    WidgetVariable          deviceWriteCommand         = new WidgetVariable<String >("device-write-command",          ""   );

    WidgetVariable          serverPort                 = new WidgetVariable<Integer>("server-port",                   0    );
//    WidgetVariable          serverTLSPort              = new WidgetVariable<Integer>("server-tls-port",               0    );
    WidgetVariable          serverCAFile               = new WidgetVariable<String >("server-ca-file",                ""   );
    WidgetVariable          serverCertFile             = new WidgetVariable<String >("server-cert-file",              ""   );
    WidgetVariable          serverKeyFile              = new WidgetVariable<String >("server-key-file",               ""   );
    WidgetVariable          serverPassword             = new WidgetVariable<String >("server-password",               ""   );

    WidgetVariable          verbose                    = new WidgetVariable<Integer>("verbose",                       0    );
    WidgetVariable          log                        = new WidgetVariable<String >("log",                           ""   );
    WidgetVariable          logFile                    = new WidgetVariable<String >("log-file",                      ""   );
    WidgetVariable          logFormat                  = new WidgetVariable<String >("log-format",                    ""   );
    WidgetVariable          logPostCommand             = new WidgetVariable<String >("log-post-command",              ""   );

    final MaintenanceDataComparator maintenanceDataComparator;
    final ServerDataComparator      serverDataComparator;

    // get display
    display = shell.getDisplay();

    // get colors

    // get images
    IMAGE_TOGGLE_MARK = Widgets.loadImage(display,"togglemark.png");

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Server settings"),700,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    tabFolder = Widgets.newTabFolder(dialog);
    Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);

    final Table  widgetMaintenanceTable;
    final Button widgetAddMaintenance;
    final Button widgetEditMaintenance;
    final Button widgetRemoveMaintenance;

    final Table  widgetServerTable;
    final Button widgetAddServer;
    final Button widgetEditServer;
    final Button widgetRemoveServer;
    final Button widgetSave;

    // general
    composite = Widgets.addTab(tabFolder,BARControl.tr("General"));
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,4);
    {
      row = 0;

      label = Widgets.newLabel(composite,BARControl.tr("Temporary directory")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,0.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        subSubComposite = BARWidgets.newDirectory(subComposite,
                                                  BARControl.tr("Path to temporary directory."),
                                                  tmpDirectory
                                                 );
        Widgets.layout(subSubComposite,0,0,TableLayoutData.WE);

        label = Widgets.newLabel(subComposite,BARControl.tr("Max. size")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        combo = BARWidgets.newByteSize(subComposite,
                                       BARControl.tr("Size limit for temporary files."),
                                       maxTmpSize,
                                       new String[]{"","32M","64M","128M","140M","256M","512M","1G","2G","4G","8G","64G","128G","512G","1T","2T","4T","8T"}
                                      );
        Widgets.layout(combo,0,2,TableLayoutData.WE);
      }
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Nice level")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Process nice level."),
                                     niceLevel,
                                     0,
                                     19
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Max. number of threads")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Max. number of compression and encryption threads."),
                                     maxThreads,
                                     0,
                                     65535
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

/*
      label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      combo = BARWidgets.newByteSize(composite,
                                     BARControl.tr("Max. band width to use [bits/s]."),
                                     maxBandWidth,
                                     new String[]{"0","64K","128K","256K","512K","1M","2M","4M","8M","16M","32M","64M","128M","256M","512M","1G","10G"}
                                    );
      Widgets.layout(combo,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;
*/

      label = Widgets.newLabel(composite,BARControl.tr("Min. compress size")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        combo = BARWidgets.newByteSize(subComposite,
                                       BARControl.tr("Min. size of files for compression."),
                                       compressMinSize,
                                       new String[]{"0","32","64","128","256","512","1K","4K","8K"}
                                      );
        Widgets.layout(combo,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
        label = Widgets.newLabel(subComposite,BARControl.tr("bytes"));
        Widgets.layout(label,0,1,TableLayoutData.W);
      }
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Jobs directory")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newDirectory(composite,
                                                BARControl.tr("Jobs directory."),
                                                serverJobsDirectory
                                               );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Max. continuous entry size")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        combo = BARWidgets.newByteSize(subComposite,
                                       BARControl.tr("Size limit for continuous stored entries."),
                                       continuousMaxSize,
                                       new String[]{"32M","64M","128M","140M","256M","512M","1G","2G","4G","8G","64G","128G","512G","1T","2T","4T","8T"}
                                      );
        Widgets.layout(combo,0,0,TableLayoutData.WE);
        label = Widgets.newLabel(subComposite,BARControl.tr("bytes"));
        Widgets.layout(label,0,1,TableLayoutData.W);
      }
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Index database")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("Index database."),
                                           indexDatabase,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      button = BARWidgets.newCheckbox(composite,
                                      BARControl.tr("Run requested index database updates."),
                                      indexDatabaseUpdate,
                                      BARControl.tr("Index update")
                                     );
      Widgets.layout(button,row,1,TableLayoutData.W);
      row++;

      button = BARWidgets.newCheckbox(composite,
                                      BARControl.tr("Auto add lost archives to index database."),
                                      indexDatabaseAutoUpdate,
                                      BARControl.tr("Auto index update")
                                     );
      Widgets.layout(button,row,1,TableLayoutData.W);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Index database keep time")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        combo = BARWidgets.newTime(subComposite,
                                   BARControl.tr("Index database keep time for not existing storages."),
                                   indexDatabaseKeepTime,
                                   new String[]{"",
                                                BARControl.tr("1 day"),
                                                BARControl.tr("3 days"),
                                                BARControl.tr("1 week"),
                                                BARControl.tr("2 weeks"),
                                                BARControl.tr("3 weeks"),
                                                BARControl.tr("30 days")
                                               }
                                  );
        Widgets.layout(combo,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      }
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Maintenance")+":");
      Widgets.layout(label,row,0,TableLayoutData.NW);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,2));
      Widgets.layout(subComposite,row,1,TableLayoutData.NSWE);
      {
        widgetMaintenanceTable = Widgets.newTable(subComposite);
        Widgets.layout(widgetMaintenanceTable,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,200);
        tableColumn = Widgets.addTableColumn(widgetMaintenanceTable,0,BARControl.tr("Date"),     SWT.LEFT,120,false);
        tableColumn.setToolTipText(BARControl.tr("Click to sort by date."));
        tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
        tableColumn = Widgets.addTableColumn(widgetMaintenanceTable,0,BARControl.tr("Week days"),SWT.LEFT,250,false);
        tableColumn.setToolTipText(BARControl.tr("Click to sort by week days."));
        tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
        tableColumn = Widgets.addTableColumn(widgetMaintenanceTable,1,BARControl.tr("Begin"),    SWT.LEFT,120,true );
        tableColumn.setToolTipText(BARControl.tr("Click to sort by begin time."));
        tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
        tableColumn = Widgets.addTableColumn(widgetMaintenanceTable,2,BARControl.tr("End"),      SWT.LEFT,120,true );
        tableColumn.setToolTipText(BARControl.tr("Click to sort by end time."));
        tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
        maintenanceDataComparator = new MaintenanceDataComparator(widgetMaintenanceTable);

        subSubComposite = Widgets.newComposite(subComposite);
        subSubComposite.setLayout(new TableLayout(0.0,0.0));
        Widgets.layout(subSubComposite,1,0,TableLayoutData.E);
        {
          widgetAddMaintenance = Widgets.newButton(subSubComposite,BARControl.tr("Add")+"\u2026");
          Widgets.layout(widgetAddMaintenance,0,0,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
          widgetAddMaintenance.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MaintenanceData maintenanceData = new MaintenanceData();
              if (maintenanceEdit(dialog,maintenanceData,BARControl.tr("Add maintenance time"),BARControl.tr("Add")))
              {
                try
                {
                  ValueMap resultMap = new ValueMap();

                  BARServer.executeCommand(StringParser.format("MAINTENANCE_LIST_ADD date=%s weekDays=%s beginTime=%s endTime=%s",
                                                               maintenanceData.getDate(),
                                                               maintenanceData.getWeekDays(),
                                                               maintenanceData.getBeginTime(),
                                                               maintenanceData.getEndTime()
                                                              ),
                                           0,  // debugLevel
                                           resultMap
                                          );
                  maintenanceData.id = resultMap.getInt("id");

                  Widgets.insertTableItem(widgetMaintenanceTable,
                                          maintenanceDataComparator,
                                          maintenanceData,
                                          maintenanceData.getDate(),
                                          maintenanceData.getWeekDays(),
                                          maintenanceData.getBeginTime(),
                                          maintenanceData.getEndTime()
                                         );
                }
                catch (Exception exception)
                {
                  Dialogs.error(dialog,BARControl.tr("Add maintenance time fail:\n\n{0}",exception.getMessage()));
                }
              }
            }
          });

          widgetEditMaintenance = Widgets.newButton(subSubComposite,BARControl.tr("Edit")+"\u2026");
          Widgets.layout(widgetEditMaintenance,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
          widgetEditMaintenance.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              int index = widgetMaintenanceTable.getSelectionIndex();
              if (index >= 0)
              {
                TableItem       tableItem       = widgetMaintenanceTable.getItem(index);
                MaintenanceData maintenanceData = (MaintenanceData)tableItem.getData();

                if (maintenanceEdit(dialog,maintenanceData,BARControl.tr("Edit maintenance time"),BARControl.tr("Save")))
                {
                  try
                  {
                    BARServer.executeCommand(StringParser.format("MAINTENANCE_LIST_UPDATE id=%d date=%s weekDays=%s beginTime=%s endTime=%s",
                                                                 maintenanceData.id,
                                                                 maintenanceData.getDate(),
                                                                 maintenanceData.getWeekDays(),
                                                                 maintenanceData.getBeginTime(),
                                                                 maintenanceData.getEndTime()
                                                                ),
                                             0  // debugLevel
                                            );

                    Widgets.updateTableItem(tableItem,
                                            maintenanceData,
                                            maintenanceData.getDate(),
                                            maintenanceData.getWeekDays(),
                                            maintenanceData.getBeginTime(),
                                            maintenanceData.getEndTime()
                                           );
                  }
                  catch (Exception exception)
                  {
                    Dialogs.error(dialog,BARControl.tr("Save maintenance time fail:\n\n{0}",exception.getMessage()));
                  }
                }
              }
            }
          });

          button = Widgets.newButton(subSubComposite,BARControl.tr("Clone")+"\u2026");
          Widgets.layout(button,0,2,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              int index = widgetMaintenanceTable.getSelectionIndex();
              if (index >= 0)
              {
                TableItem       tableItem       = widgetMaintenanceTable.getItem(index);
                MaintenanceData maintenanceData = (MaintenanceData)tableItem.getData();

                MaintenanceData cloneMaintenanceData = (MaintenanceData)maintenanceData.clone();
                if (maintenanceEdit(dialog,cloneMaintenanceData,BARControl.tr("Clone maintenance time"),BARControl.tr("Add")))
                {
                  try
                  {
                    ValueMap resultMap = new ValueMap();

                    BARServer.executeCommand(StringParser.format("MAINTENANCE_LIST_ADD date=%s weekDays=%s beginTime=%s endTime=%s",
                                                                 cloneMaintenanceData.getDate(),
                                                                 cloneMaintenanceData.getWeekDays(),
                                                                 cloneMaintenanceData.getBeginTime(),
                                                                 cloneMaintenanceData.getEndTime()
                                                                ),
                                             0,  // debugLevel
                                             resultMap
                                            );

                    cloneMaintenanceData.id = resultMap.getInt("id");

                    Widgets.insertTableItem(widgetMaintenanceTable,
                                            maintenanceDataComparator,
                                            cloneMaintenanceData,
                                            cloneMaintenanceData.getDate(),
                                            cloneMaintenanceData.getWeekDays(),
                                            cloneMaintenanceData.getBeginTime(),
                                            cloneMaintenanceData.getEndTime()
                                           );
                  }
                  catch (Exception exception)
                  {
                    Dialogs.error(dialog,BARControl.tr("Add maintenance time fail:\n\n{0}",exception.getMessage()));
                  }
                }
              }
            }
          });

          widgetRemoveMaintenance = Widgets.newButton(subSubComposite,BARControl.tr("Remove")+"\u2026");
          Widgets.layout(widgetRemoveMaintenance,0,3,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
          widgetRemoveMaintenance.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              int index = widgetMaintenanceTable.getSelectionIndex();
              if (index >= 0)
              {
                TableItem       tableItem       = widgetMaintenanceTable.getItem(index);
                MaintenanceData maintenanceData = (MaintenanceData)tableItem.getData();

                if (Dialogs.confirm(dialog,BARControl.tr("Delete maintenance time {0}, {1}..{2}?",maintenanceData.getDate(),maintenanceData.getBeginTime(),maintenanceData.getEndTime())))
                {
                  try
                  {
                    ValueMap resultMap = new ValueMap();

                    BARServer.executeCommand(StringParser.format("MAINTENANCE_LIST_REMOVE id=%d",
                                                                 maintenanceData.id
                                                                ),
                                             0,  // debugLevel
                                             resultMap
                                            );
                    Widgets.removeTableItem(widgetMaintenanceTable,
                                            tableItem
                                           );
                  }
                  catch (Exception exception)
                  {
                    Dialogs.error(dialog,BARControl.tr("Delete maintenance time fail:\n\n{0}",exception.getMessage()));
                  }
                }
              }
            }
          });
        }
      }
      row++;
    }

    // servers
    composite = Widgets.addTab(tabFolder,BARControl.tr("Servers"));
    composite.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,0.0,0.0,0.0,1.0},new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      row = 0;

      label = Widgets.newLabel(composite,BARControl.tr("Port")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("BAR server port number."),
                                     serverPort,
                                     0,
                                     65535
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,70,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("CA file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("BAR certificate authority file."),
                                           serverCAFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Cert file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("BAR certificate file."),
                                           serverCertFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Key file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("BAR key file."),
                                           serverKeyFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Password")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      text = BARWidgets.newPassword(composite,
                                    BARControl.tr("BAR server password."),
                                    serverPassword
                                   );
      Widgets.layout(text,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Storage servers")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      row++;

      widgetServerTable = Widgets.newTable(composite);
      Widgets.layout(widgetServerTable,row,0,TableLayoutData.NSWE,0,2);
      tableColumn = Widgets.addTableColumn(widgetServerTable,0,BARControl.tr("Type"),SWT.LEFT, 60,false);
      tableColumn.setToolTipText(BARControl.tr("Click to sort by type."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      tableColumn = Widgets.addTableColumn(widgetServerTable,1,BARControl.tr("Name"),SWT.LEFT,512,true );
      tableColumn.setToolTipText(BARControl.tr("Click to sort by name."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      tableColumn = Widgets.addTableColumn(widgetServerTable,2,"#",SWT.LEFT, 32,false);
      tableColumn.setToolTipText(BARControl.tr("Click to sort by max. number of concurrent connections."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      tableColumn = Widgets.addTableColumn(widgetServerTable,3,BARControl.tr("Size"),SWT.LEFT, 64,false);
      tableColumn.setToolTipText(BARControl.tr("Click to sort by max. storage size."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      row++;
      serverDataComparator = new ServerDataComparator(widgetServerTable);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(1.0,0.0,2));
      Widgets.layout(subComposite,row,0,TableLayoutData.E,0,2);
      {
        widgetAddServer = Widgets.newButton(subComposite,BARControl.tr("Add")+"\u2026");
        Widgets.layout(widgetAddServer,0,0,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        widgetAddServer.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            ServerData serverData = new ServerData();
            serverData.type = ServerTypes.FILE;
            if (serverEdit(dialog,serverData,BARControl.tr("Add storage server"),BARControl.tr("Add")))
            {
              try
              {
                ValueMap resultMap = new ValueMap();

                BARServer.executeCommand(StringParser.format("SERVER_LIST_ADD name=%'S serverType=%s loginName=%'S port=%d password=%'S publicKey=%'S privateKey=%'S maxConnectionCount=%d maxStorageSize=%ld",
                                                             serverData.name,
                                                             serverData.type,
                                                             serverData.loginName,
                                                             serverData.port,
                                                             serverData.password,
                                                             serverData.publicKey,
                                                             serverData.privateKey,
                                                             serverData.maxConnectionCount,
                                                             serverData.maxStorageSize
                                                            ),
                                         0,  // debugLevel
                                         resultMap
                                        );
                serverData.id = resultMap.getInt("id");

                Widgets.insertTableItem(widgetServerTable,
                                        serverDataComparator,
                                        serverData,
                                        serverData.type.toString(),
                                        serverData.name,
                                        (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                                        (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                                       );
              }
              catch (Exception exception)
              {
                Dialogs.error(dialog,BARControl.tr("Add storage server fail:\n\n{0}",exception.getMessage()));
              }
            }
          }
        });

        widgetEditServer = Widgets.newButton(subComposite,BARControl.tr("Edit")+"\u2026");
        Widgets.layout(widgetEditServer,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        widgetEditServer.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int index = widgetServerTable.getSelectionIndex();
            if (index >= 0)
            {
              TableItem  tableItem  = widgetServerTable.getItem(index);
              ServerData serverData = (ServerData)tableItem.getData();

              if (serverEdit(dialog,serverData,BARControl.tr("Edit storage server"),BARControl.tr("Save")))
              {
                try
                {
                  BARServer.executeCommand(StringParser.format("SERVER_LIST_UPDATE id=%d name=%'S serverType=%s loginName=%'S port=%d password=%'S publicKey=%'S privateKey=%'S maxConnectionCount=%d maxStorageSize=%ld",
                                                               serverData.id,
                                                               serverData.name,
                                                               serverData.type,
                                                               serverData.loginName,
                                                               serverData.port,
                                                               serverData.password,
                                                               serverData.publicKey,
                                                               serverData.privateKey,
                                                               serverData.maxConnectionCount,
                                                               serverData.maxStorageSize
                                                              ),
                                           0  // debugLevel
                                          );

                  Widgets.updateTableItem(tableItem,
                                          serverData,
                                          serverData.type.toString(),
                                          serverData.name,
                                          (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                                          (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                                         );
                }
                catch (Exception exception)
                {
                  Dialogs.error(dialog,BARControl.tr("Save storage server settings fail:\n\n{0}",exception.getMessage()));
                }
              }
            }
          }
        });

        button = Widgets.newButton(subComposite,BARControl.tr("Clone")+"\u2026");
        Widgets.layout(button,0,2,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int index = widgetServerTable.getSelectionIndex();
            if (index >= 0)
            {
              TableItem  tableItem  = widgetServerTable.getItem(index);
              ServerData serverData = (ServerData)tableItem.getData();

              ServerData cloneServerData = (ServerData)serverData.clone();
              if (serverEdit(dialog,cloneServerData,BARControl.tr("Clone storage server"),BARControl.tr("Add")))
              {
                try
                {
                  ValueMap resultMap = new ValueMap();

                  BARServer.executeCommand(StringParser.format("SERVER_LIST_ADD name=%'S serverType=%s loginName=%'S port=%d password=%'S publicKey=%'S privateKey=%'S maxConnectionCount=%d maxStorageSize=%ld",
                                                               cloneServerData.name,
                                                               cloneServerData.type,
                                                               cloneServerData.loginName,
                                                               cloneServerData.port,
                                                               cloneServerData.password,
                                                               cloneServerData.publicKey,
                                                               cloneServerData.privateKey,
                                                               cloneServerData.maxConnectionCount,
                                                               cloneServerData.maxStorageSize
                                                              ),
                                           0,  // debugLevel
                                           resultMap
                                          );

                  cloneServerData.id = resultMap.getInt("id");

                  Widgets.insertTableItem(widgetServerTable,
                                          serverDataComparator,
                                          cloneServerData,
                                          cloneServerData.type.toString(),
                                          cloneServerData.name,
                                          (cloneServerData.maxConnectionCount > 0) ? cloneServerData.maxConnectionCount : "-",
                                          (cloneServerData.maxStorageSize > 0) ? Units.formatByteSize(cloneServerData.maxStorageSize) : "-"
                                         );
                }
                catch (Exception exception)
                {
                  Dialogs.error(dialog,BARControl.tr("Add storage server fail:\n\n{0}",exception.getMessage()));
                }
              }
            }
          }
        });

        widgetRemoveServer = Widgets.newButton(subComposite,BARControl.tr("Remove")+"\u2026");
        Widgets.layout(widgetRemoveServer,0,3,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        widgetRemoveServer.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int index = widgetServerTable.getSelectionIndex();
            if (index >= 0)
            {
              TableItem  tableItem  = widgetServerTable.getItem(index);
              ServerData serverData = (ServerData)tableItem.getData();

              if (Dialogs.confirm(dialog,BARControl.tr("Delete settings of storage server ''{0}''?",serverData.name)))
              {
                try
                {
                  ValueMap resultMap = new ValueMap();

                  BARServer.executeCommand(StringParser.format("SERVER_LIST_REMOVE id=%d",
                                                               serverData.id
                                                              ),
                                           0,  // debugLevel
                                           resultMap
                                          );
                  Widgets.removeTableItem(widgetServerTable,
                                          tableItem
                                         );
                }
                catch (Exception exception)
                {
                  Dialogs.error(dialog,BARControl.tr("Delete storage server settings fail:\n\n{0}",exception.getMessage()));
                }
              }
            }
          }
        });
      }
      row++;
    }

    // commands
    composite = Widgets.addTab(tabFolder,BARControl.tr("Commands"));
    composite.setLayout(new TableLayout(1.0,1.0,2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,4);
    {
      subTabFolder = Widgets.newTabFolder(composite);
      Widgets.layout(subTabFolder,0,0,TableLayoutData.NSWE);

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("CD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD device name."),
                                             cdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request medium command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request medium command."),
                                             cdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload CD."),
                                             cdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load CD."),
                                             cdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating CD image."),
                                             cdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating CD image."),
                                             cdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             cdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             cdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             cdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             cdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Blank command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to blank medium."),
                                             cdBlankCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing CD."),
                                             cdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing CD."),
                                             cdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD write command."),
                                             cdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD image write command."),
                                             cdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("DVD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;
        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD device name."),
                                             dvdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request medium command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request medium command."),
                                             dvdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload DVD."),
                                             dvdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load DVD."),
                                             dvdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating DVD image."),
                                             dvdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating DVD image."),
                                             dvdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             dvdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             dvdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             dvdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             dvdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Blank command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to blank medium."),
                                             dvdBlankCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing DVD."),
                                             dvdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing DVD."),
                                             dvdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD write command."),
                                             dvdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD image write command."),
                                             dvdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("BD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD device name."),
                                             bdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request medium command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request medium command."),
                                             bdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload BD."),
                                             bdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load BD."),
                                             bdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating BD image."),
                                             bdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating BD image."),
                                             bdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             bdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             bdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             bdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             bdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Blank command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to blank medium."),
                                             bdBlankCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing BD."),
                                             bdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing BD."),
                                             bdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD write command."),
                                             bdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD image write command."),
                                             bdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.trc("Device (tab)","Device"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.trc("Device (content)","Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Device name."),
                                             deviceName,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request medium command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request medium command."),
                                             deviceRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload device."),
                                             deviceUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load device."),
                                             deviceLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating device image."),
                                             deviceImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating device image."),
                                             deviceImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             deviceImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             deviceECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             deviceECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             deviceECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Blank command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to blank medium."),
                                             deviceBlankCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing device."),
                                             deviceWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing device."),
                                             deviceWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("device write command."),
                                             deviceWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }
    }

    // output+log
    composite = Widgets.addTab(tabFolder,BARControl.tr("Output && Log"));
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Verbosity level")+":");
      Widgets.layout(label,0,0,TableLayoutData.NW);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Verbosity level."),
                                     verbose,
                                     0,
                                     6
                                    );
      Widgets.layout(spinner,0,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      label = Widgets.newLabel(composite,BARControl.tr("Log")+":");
      Widgets.layout(label,1,0,TableLayoutData.NW);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,0.0}));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        row = 0;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log errors."),
                                        log,
                                        BARControl.tr("errors"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("errors");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("errors");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("errors");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log warnings."),
                                        log,
                                        BARControl.tr("warnings"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("warnings");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("warnings");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("warnings");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log info."),
                                        log,
                                        BARControl.tr("info"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("info");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("info");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("info");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log stored/restored files."),
                                        log,
                                        BARControl.tr("ok"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("ok");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("ok");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("ok");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log unknown files."),
                                        log,
                                        BARControl.tr("unknown files"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("unknown");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("unknown");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("unknown");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log skipped files."),
                                        log,
                                        BARControl.tr("skipped files"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("skipped");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("skipped");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("skipped");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log missing files."),
                                        log,
                                        BARControl.tr("missing files"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("missing");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("missing");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("missing");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log incomplete files."),
                                        log,
                                        BARControl.tr("incomplete files"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("incomplete");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("incomplete");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("incomplete");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log excluded files."),
                                        log,
                                        BARControl.tr("excluded files"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("excluded");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("excluded");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("excluded");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log storage operations."),
                                        log,
                                        BARControl.tr("storage operations"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("storage");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("storage");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("storage");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log index operations."),
                                        log,
                                        BARControl.tr("index operations"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("index");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("index");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("index");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log continuous operations."),
                                        log,
                                        BARControl.tr("continuous operations"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("continuous");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("continuous");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("continuous");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("all"),
                                        log,
                                        BARControl.tr("all"),
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.splitArray(widgetVariable.getString(),",")));

                                            return values.contains("all");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            if (checked)
                                            {
                                              widgetVariable.set("all");
                                            }
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;
      }

      label = Widgets.newLabel(composite,BARControl.tr("Log file")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      subComposite = BARWidgets.newFile(composite,
                                        BARControl.tr("Log file name."),
                                        logFile,
                                        new String[]{BARControl.tr("Log files"),"*.log",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*.log"
                                       );
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Log format")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);
      text = BARWidgets.newText(composite,
                                BARControl.tr("Log format string."),
                                logFormat
                               );
      Widgets.layout(text,3,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Log post command")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);
      subComposite = BARWidgets.newFile(composite,
                                        BARControl.tr("Log post command."),
                                        logPostCommand,
                                        new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*"
                                       );
      Widgets.layout(subComposite,4,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetSave = Widgets.newButton(composite,BARControl.tr("Save"));
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetMaintenanceTable.addMouseListener(new MouseListener()
    {
      public void mouseDoubleClick(final MouseEvent mouseEvent)
      {
        Widgets.invoke(widgetEditMaintenance);
      }
      public void mouseDown(final MouseEvent mouseEvent)
      {
      }
      public void mouseUp(final MouseEvent mouseEvent)
      {
      }
    });
    widgetMaintenanceTable.addKeyListener(new KeyListener()
    {
      public void keyPressed(KeyEvent keyEvent)
      {
      }
      public void keyReleased(KeyEvent keyEvent)
      {
        if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
        {
          Widgets.invoke(widgetAddMaintenance);
        }
        else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
        {
          Widgets.invoke(widgetRemoveMaintenance);
        }
        else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
        {
          Widgets.invoke(widgetEditMaintenance);
        }
      }
    });
    widgetServerTable.addMouseListener(new MouseListener()
    {
      public void mouseDoubleClick(final MouseEvent mouseEvent)
      {
        Widgets.invoke(widgetEditServer);
      }
      public void mouseDown(final MouseEvent mouseEvent)
      {
      }
      public void mouseUp(final MouseEvent mouseEvent)
      {
      }
    });
    widgetServerTable.addKeyListener(new KeyListener()
    {
      public void keyPressed(KeyEvent keyEvent)
      {
      }
      public void keyReleased(KeyEvent keyEvent)
      {
        if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
        {
          Widgets.invoke(widgetAddServer);
        }
        else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
        {
          Widgets.invoke(widgetRemoveServer);
        }
        else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
        {
          Widgets.invoke(widgetEditServer);
        }
      }
    });
    widgetSave.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,true);
      }
    });

    BARServer.getServerOption(tmpDirectory               );
    BARServer.getServerOption(maxTmpSize                 );
    BARServer.getServerOption(niceLevel                  );
    BARServer.getServerOption(maxThreads                 );
//    BARServer.getServerOption(maxBandWidth               );
    BARServer.getServerOption(compressMinSize            );

    BARServer.getServerOption(continuousMaxSize          );

    BARServer.getServerOption(indexDatabase              );
    BARServer.getServerOption(indexDatabaseUpdate        );
    BARServer.getServerOption(indexDatabaseAutoUpdate    );
//    BARServer.getServerOption(indexDatabaseMaxBandWidth  );
    BARServer.getServerOption(indexDatabaseKeepTime      );

    BARServer.getServerOption(cdDevice                   );
    BARServer.getServerOption(cdRequestVolumeCommand     );
    BARServer.getServerOption(cdUnloadCommand            );
    BARServer.getServerOption(cdLoadCommand              );
    BARServer.getServerOption(cdVolumeSize               );
    BARServer.getServerOption(cdImagePreCommand          );
    BARServer.getServerOption(cdImagePostCommand         );
    BARServer.getServerOption(cdImageCommandCommand      );
    BARServer.getServerOption(cdECCPreCommand            );
    BARServer.getServerOption(cdECCPostCommand           );
    BARServer.getServerOption(cdECCCommand               );
    BARServer.getServerOption(cdBlankCommand             );
    BARServer.getServerOption(cdWritePreCommand          );
    BARServer.getServerOption(cdWritePostCommand         );
    BARServer.getServerOption(cdWriteCommand             );
    BARServer.getServerOption(cdWriteImageCommand        );

    BARServer.getServerOption(dvdDevice                  );
    BARServer.getServerOption(dvdRequestVolumeCommand    );
    BARServer.getServerOption(dvdUnloadCommand           );
    BARServer.getServerOption(dvdLoadCommand             );
    BARServer.getServerOption(dvdVolumeSize              );
    BARServer.getServerOption(dvdImagePreCommand         );
    BARServer.getServerOption(dvdImagePostCommand        );
    BARServer.getServerOption(dvdImageCommandCommand     );
    BARServer.getServerOption(dvdECCPreCommand           );
    BARServer.getServerOption(dvdECCPostCommand          );
    BARServer.getServerOption(dvdECCCommand              );
    BARServer.getServerOption(dvdBlankCommand            );
    BARServer.getServerOption(dvdWritePreCommand         );
    BARServer.getServerOption(dvdWritePostCommand        );
    BARServer.getServerOption(dvdWriteCommand            );
    BARServer.getServerOption(dvdWriteImageCommand       );

    BARServer.getServerOption(bdDevice                   );
    BARServer.getServerOption(bdRequestVolumeCommand     );
    BARServer.getServerOption(bdUnloadCommand            );
    BARServer.getServerOption(bdLoadCommand              );
    BARServer.getServerOption(bdVolumeSize               );
    BARServer.getServerOption(bdImagePreCommand          );
    BARServer.getServerOption(bdImagePostCommand         );
    BARServer.getServerOption(bdImageCommandCommand      );
    BARServer.getServerOption(bdECCPreCommand            );
    BARServer.getServerOption(bdECCPostCommand           );
    BARServer.getServerOption(bdECCCommand               );
    BARServer.getServerOption(bdBlankCommand             );
    BARServer.getServerOption(bdWritePreCommand          );
    BARServer.getServerOption(bdWritePostCommand         );
    BARServer.getServerOption(bdWriteCommand             );
    BARServer.getServerOption(bdWriteImageCommand        );

    BARServer.getServerOption(deviceName                 );
    BARServer.getServerOption(deviceRequestVolumeCommand );
    BARServer.getServerOption(deviceUnloadCommand        );
    BARServer.getServerOption(deviceLoadCommand          );
    BARServer.getServerOption(deviceVolumeSize           );
    BARServer.getServerOption(deviceImagePreCommand      );
    BARServer.getServerOption(deviceImagePostCommand     );
    BARServer.getServerOption(deviceImageCommandCommand  );
    BARServer.getServerOption(deviceECCPreCommand        );
    BARServer.getServerOption(deviceECCPostCommand       );
    BARServer.getServerOption(deviceECCCommand           );
    BARServer.getServerOption(deviceBlankCommand         );
    BARServer.getServerOption(deviceWritePreCommand      );
    BARServer.getServerOption(deviceWritePostCommand     );
    BARServer.getServerOption(deviceWriteCommand         );

    BARServer.getServerOption(serverPort                 );
//    BARServer.getServerOption(serverTLSPort              );
    BARServer.getServerOption(serverCAFile               );
    BARServer.getServerOption(serverCertFile             );
    BARServer.getServerOption(serverKeyFile              );
    BARServer.getServerOption(serverPassword             );
    BARServer.getServerOption(serverJobsDirectory        );

    BARServer.getServerOption(verbose                    );
    BARServer.getServerOption(log                        );
    BARServer.getServerOption(logFile                    );
    BARServer.getServerOption(logFormat                  );
    BARServer.getServerOption(logPostCommand             );

    Widgets.removeAllTableItems(widgetMaintenanceTable);
    try
    {
      BARServer.executeCommand(StringParser.format("MAINTENANCE_LIST"),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                   throws BARException
                                 {
                                   // get data
                                   int    id        = valueMap.getInt   ("id"       );
                                   String date      = valueMap.getString("date"     );
                                   String weekDays  = valueMap.getString("weekDays" );
                                   String beginTime = valueMap.getString("beginTime");
                                   String endTime   = valueMap.getString("endTime"  );

                                   // create server data
                                   MaintenanceData maintenanceData = new MaintenanceData(id,date,weekDays,beginTime,endTime);

                                   // add table entry
                                   Widgets.insertTableItem(widgetMaintenanceTable,
                                                           maintenanceDataComparator,
                                                           maintenanceData,
                                                           maintenanceData.getDate(),
                                                           maintenanceData.getWeekDays(),
                                                           maintenanceData.getBeginTime(),
                                                           maintenanceData.getEndTime()
                                                          );
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(dialog,BARControl.tr("Get maintenance list fail:\n\n{0}",exception.getMessage()));
      return;
    }

    Widgets.removeAllTableItems(widgetServerTable);
    ServerData serverData;
    serverData = new ServerData(0,"default",ServerTypes.FTP);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            serverData,
                            ServerTypes.FTP.toString(),
                            "default",
                            (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                            (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                           );
    serverData = new ServerData(0,"default",ServerTypes.SSH);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            serverData,
                            ServerTypes.SSH.toString(),
                            "default",
                            (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                            (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                           );
    serverData = new ServerData(0,"default",ServerTypes.WEBDAV);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            serverData,
                            ServerTypes.WEBDAV.toString(),
                            "default",
                            (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                            (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                           );

    try
    {
      BARServer.executeCommand(StringParser.format("SERVER_LIST"),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                   throws BARException
                                 {
                                   // get data
                                   int         id                 = valueMap.getInt   ("id"                          );
                                   String      name               = valueMap.getString("name"                        );
                                   ServerTypes serverType         = valueMap.getEnum  ("serverType",ServerTypes.class);
                                   int         port               = valueMap.getInt   ("port",0                      );
                                   String      loginName          = valueMap.getString("loginName",""                );
                                   int         maxConnectionCount = valueMap.getInt   ("maxConnectionCount",0        );
                                   long        maxStorageSize     = valueMap.getLong  ("maxStorageSize"              );

                                   // create server data
                                   ServerData serverData = new ServerData(id,name,serverType,loginName,port,maxConnectionCount,maxStorageSize);

                                   // add table entry
                                   Widgets.insertTableItem(widgetServerTable,
                                                           serverDataComparator,
                                                           serverData,
                                                           serverType.toString(),
                                                           serverData.name,
                                                           (serverData.maxConnectionCount > 0) ? serverData.maxConnectionCount : "-",
                                                           (serverData.maxStorageSize > 0) ? Units.formatByteSize(serverData.maxStorageSize) : "-"
                                                          );
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(dialog,BARControl.tr("Get server list fail:\n\n{0}",exception.getMessage()));
      return;
    }

    if ((Boolean)Dialogs.run(dialog,false))
    {
      try
      {
        BARServer.setServerOption(tmpDirectory               );
        BARServer.setServerOption(maxTmpSize                 );
        BARServer.setServerOption(niceLevel                  );
        BARServer.setServerOption(maxThreads                 );
  //      BARServer.setServerOption(maxBandWidth);
        BARServer.setServerOption(compressMinSize            );

        BARServer.setServerOption(continuousMaxSize          );

        BARServer.setServerOption(indexDatabase);
        BARServer.setServerOption(indexDatabaseUpdate        );
        BARServer.setServerOption(indexDatabaseAutoUpdate    );
  //      BARServer.setServerOption(indexDatabaseMaxBandWidth);
        BARServer.setServerOption(indexDatabaseKeepTime      );

        BARServer.setServerOption(cdDevice                   );
        BARServer.setServerOption(cdRequestVolumeCommand     );
        BARServer.setServerOption(cdUnloadCommand            );
        BARServer.setServerOption(cdLoadCommand              );
        BARServer.setServerOption(cdVolumeSize               );
        BARServer.setServerOption(cdImagePreCommand          );
        BARServer.setServerOption(cdImagePostCommand         );
        BARServer.setServerOption(cdImageCommandCommand      );
        BARServer.setServerOption(cdECCPreCommand            );
        BARServer.setServerOption(cdECCPostCommand           );
        BARServer.setServerOption(cdECCCommand               );
        BARServer.setServerOption(cdBlankCommand             );
        BARServer.setServerOption(cdWritePreCommand          );
        BARServer.setServerOption(cdWritePostCommand         );
        BARServer.setServerOption(cdWriteCommand             );
        BARServer.setServerOption(cdWriteImageCommand        );

        BARServer.setServerOption(dvdDevice                  );
        BARServer.setServerOption(dvdRequestVolumeCommand    );
        BARServer.setServerOption(dvdUnloadCommand           );
        BARServer.setServerOption(dvdLoadCommand             );
        BARServer.setServerOption(dvdVolumeSize              );
        BARServer.setServerOption(dvdImagePreCommand         );
        BARServer.setServerOption(dvdImagePostCommand        );
        BARServer.setServerOption(dvdImageCommandCommand     );
        BARServer.setServerOption(dvdECCPreCommand           );
        BARServer.setServerOption(dvdECCPostCommand          );
        BARServer.setServerOption(dvdECCCommand              );
        BARServer.setServerOption(dvdBlankCommand            );
        BARServer.setServerOption(dvdWritePreCommand         );
        BARServer.setServerOption(dvdWritePostCommand        );
        BARServer.setServerOption(dvdWriteCommand            );
        BARServer.setServerOption(dvdWriteImageCommand       );

        BARServer.setServerOption(bdDevice                   );
        BARServer.setServerOption(bdRequestVolumeCommand     );
        BARServer.setServerOption(bdUnloadCommand            );
        BARServer.setServerOption(bdLoadCommand              );
        BARServer.setServerOption(bdVolumeSize               );
        BARServer.setServerOption(bdImagePreCommand          );
        BARServer.setServerOption(bdImagePostCommand         );
        BARServer.setServerOption(bdImageCommandCommand      );
        BARServer.setServerOption(bdECCPreCommand            );
        BARServer.setServerOption(bdECCPostCommand           );
        BARServer.setServerOption(bdECCCommand               );
        BARServer.setServerOption(bdBlankCommand             );
        BARServer.setServerOption(bdWritePreCommand          );
        BARServer.setServerOption(bdWritePostCommand         );
        BARServer.setServerOption(bdWriteCommand             );
        BARServer.setServerOption(bdWriteImageCommand        );

        BARServer.setServerOption(deviceName                 );
        BARServer.setServerOption(deviceRequestVolumeCommand );
        BARServer.setServerOption(deviceUnloadCommand        );
        BARServer.setServerOption(deviceLoadCommand          );
        BARServer.setServerOption(deviceVolumeSize           );
        BARServer.setServerOption(deviceImagePreCommand      );
        BARServer.setServerOption(deviceImagePostCommand     );
        BARServer.setServerOption(deviceImageCommandCommand  );
        BARServer.setServerOption(deviceECCPreCommand        );
        BARServer.setServerOption(deviceECCPostCommand       );
        BARServer.setServerOption(deviceECCCommand           );
        BARServer.setServerOption(deviceBlankCommand         );
        BARServer.setServerOption(deviceWritePreCommand      );
        BARServer.setServerOption(deviceWritePostCommand     );
        BARServer.setServerOption(deviceWriteCommand         );

        BARServer.setServerOption(serverPort                 );
//        BARServer.setServerOption(serverTLSPort              );
        BARServer.setServerOption(serverCAFile               );
        BARServer.setServerOption(serverCertFile             );
        BARServer.setServerOption(serverKeyFile              );
        BARServer.setServerOption(serverPassword             );
        BARServer.setServerOption(serverJobsDirectory        );

        BARServer.setServerOption(verbose                    );
        BARServer.setServerOption(log                        );
        BARServer.setServerOption(logFile                    );
        BARServer.setServerOption(logFormat                  );
        BARServer.setServerOption(logPostCommand             );

        BARServer.flushServerOption();
      }
      catch (Exception exception)
      {
        if (!shell.isDisposed())
        {
          Dialogs.error(shell,BARControl.tr("Flush server options fail (error: {0})",exception.getMessage()));
        }
      }
    }
  }

  /** edit mainteance time
   * @param shell shell
   * @param maintenanceData maintenance data
   * @param title window title
   * @param okText OK button text
   * @return true iff edited
   */
  private static boolean maintenanceEdit(final Shell           shell,
                                         final MaintenanceData maintenanceData,
                                         String                title,
                                         String                okText
                                        )
  {
    Composite composite,subComposite;
    Label     label;
    Button    button;

    final Shell dialog = Dialogs.openModal(shell,title,SWT.DEFAULT,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Combo    widgetYear,widgetMonth,widgetDay;
    final Button[] widgetWeekDays = new Button[7];
    final Combo    widgetBeginHour,widgetBeginMinute,widgetEndHour,widgetEndMinute;
    final Button   widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Date")+":",Settings.hasNormalRole());
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE,Settings.hasNormalRole());
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite);
        widgetYear.setToolTipText(BARControl.tr("Year to execute job. Leave to '*' for each year."));
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015","2016","2017","2018","2019","2020","2021","2022","2023","2024","2025"});
        widgetYear.setText(maintenanceData.getYear()); if (widgetYear.getText().equals("")) widgetYear.setText("*");
        if (widgetYear.getText().equals("")) widgetYear.setText("*");
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);

        widgetMonth = Widgets.newOptionMenu(subComposite);
        widgetMonth.setToolTipText(BARControl.tr("Month to execute job. Leave to '*' for each month."));
        widgetMonth.setItems(new String[]{"*","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"});
        widgetMonth.setText(maintenanceData.getMonth()); if (widgetMonth.getText().equals("")) widgetMonth.setText("*");
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);

        widgetDay = Widgets.newOptionMenu(subComposite);
        widgetDay.setToolTipText(BARControl.tr("Day to execute job. Leave to '*' for each day."));
        widgetDay.setItems(new String[]{"*","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(maintenanceData.getDay()); if (widgetDay.getText().equals("")) widgetDay.setText("*");
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Week days")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetWeekDays[MaintenanceData.MON] = Widgets.newCheckbox(subComposite,BARControl.tr("Mon"));
        widgetWeekDays[MaintenanceData.MON].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.MON],0,0,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.MON].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.MON));

        widgetWeekDays[MaintenanceData.TUE] = Widgets.newCheckbox(subComposite,BARControl.tr("Tue"));
        widgetWeekDays[MaintenanceData.TUE].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.TUE],0,1,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.TUE].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.TUE));

        widgetWeekDays[MaintenanceData.WED] = Widgets.newCheckbox(subComposite,BARControl.tr("Wed"));
        widgetWeekDays[MaintenanceData.WED].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.WED],0,2,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.WED].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.WED));

        widgetWeekDays[MaintenanceData.THU] = Widgets.newCheckbox(subComposite,BARControl.tr("Thu"));
        widgetWeekDays[MaintenanceData.THU].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.THU],0,3,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.THU].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.THU));

        widgetWeekDays[MaintenanceData.FRI] = Widgets.newCheckbox(subComposite,BARControl.tr("Fri"));
        widgetWeekDays[MaintenanceData.FRI].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.FRI],0,4,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.FRI].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.FRI));

        widgetWeekDays[MaintenanceData.SAT] = Widgets.newCheckbox(subComposite,BARControl.tr("Sat"));
        widgetWeekDays[MaintenanceData.SAT].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.SAT],0,5,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.SAT].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.SAT));

        widgetWeekDays[MaintenanceData.SUN] = Widgets.newCheckbox(subComposite,BARControl.tr("Sun"));
        widgetWeekDays[MaintenanceData.SUN].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[MaintenanceData.SUN],0,6,TableLayoutData.W);
        widgetWeekDays[MaintenanceData.SUN].setSelection(maintenanceData.weekDayIsEnabled(MaintenanceData.SUN));

        button = Widgets.newButton(subComposite,IMAGE_TOGGLE_MARK);
        button.setToolTipText(BARControl.tr("Toggle week days set."));
        Widgets.layout(button,0,7,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            for (Button button : widgetWeekDays)
            {
              button.setSelection(!button.getSelection());
            }
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Time")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetBeginHour = Widgets.newOptionMenu(subComposite);
        widgetBeginHour.setToolTipText(BARControl.tr("Hour to execute job. Leave to '*' for every hour."));
        widgetBeginHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetBeginHour.setText(maintenanceData.getBeginHour()); if (widgetBeginHour.getText().equals("")) widgetBeginHour.setText("*");
        Widgets.layout(widgetBeginHour,0,0,TableLayoutData.W);

        widgetBeginMinute = Widgets.newOptionMenu(subComposite);
        widgetBeginMinute.setToolTipText(BARControl.tr("Minute to execute job. Leave to '*' for every minute."));
        widgetBeginMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55","59"});
        widgetBeginMinute.setText(maintenanceData.getBeginMinute()); if (widgetBeginMinute.getText().equals("")) widgetBeginMinute.setText("*");
        Widgets.layout(widgetBeginMinute,0,1,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,"\u2026");
        Widgets.layout(label,0,2,TableLayoutData.W);

        widgetEndHour = Widgets.newOptionMenu(subComposite);
        widgetEndHour.setToolTipText(BARControl.tr("Hour to execute job. Leave to '*' for every hour."));
        widgetEndHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetEndHour.setText(maintenanceData.getEndHour()); if (widgetEndHour.getText().equals("")) widgetEndHour.setText("*");
        Widgets.layout(widgetEndHour,0,3,TableLayoutData.W);

        widgetEndMinute = Widgets.newOptionMenu(subComposite);
        widgetEndMinute.setToolTipText(BARControl.tr("Minute to execute job. Leave to '*' for every minute."));
        widgetEndMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55","59"});
        widgetEndMinute.setText(maintenanceData.getEndMinute()); if (widgetEndMinute.getText().equals("")) widgetEndMinute.setText("*");
        Widgets.layout(widgetEndMinute,0,4,TableLayoutData.W);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,okText);
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        maintenanceData.setDate(widgetYear.getText(),widgetMonth.getText(),widgetDay.getText());
        maintenanceData.setWeekDays(widgetWeekDays[MaintenanceData.MON].getSelection(),
                                    widgetWeekDays[MaintenanceData.TUE].getSelection(),
                                    widgetWeekDays[MaintenanceData.WED].getSelection(),
                                    widgetWeekDays[MaintenanceData.THU].getSelection(),
                                    widgetWeekDays[MaintenanceData.FRI].getSelection(),
                                    widgetWeekDays[MaintenanceData.SAT].getSelection(),
                                    widgetWeekDays[MaintenanceData.SUN].getSelection()
                                   );
        maintenanceData.setBeginTime(widgetBeginHour.getText(),widgetBeginMinute.getText());
        maintenanceData.setEndTime(widgetEndHour.getText(),widgetEndMinute.getText());

        if (maintenanceData.weekDays == MaintenanceData.NONE)
        {
          Dialogs.error(dialog,BARControl.tr("No weekdays specified!"));
          return;
        }
        if ((maintenanceData.day != MaintenanceData.ANY) && (maintenanceData.weekDays != MaintenanceData.ANY))
        {
          if (!Dialogs.confirm(dialog,
                               BARControl.tr("The maintenance time may not be used if the specified day is not in the set of specified weekdays.\nReally keep this setting?")
                              )
             )
          {
            return;
          }
        }

        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** edit storage server
   * @param shell shell
   * @param serverData storage server data
   * @param title window title
   * @param okText OK button text
   * @return true iff edited
   */
  private static boolean serverEdit(final Shell      shell,
                                    final ServerData serverData,
                                    String           title,
                                    String           okText
                                   )
  {
    Composite composite,subComposite;
    Label     label;
    Button    button;

    final Shell dialog = Dialogs.openModal(shell,title,600,500,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text       widgetName;
    final Combo      widgetType;
    final Text       widgetLoginName;
    final Spinner    widgetPort;
    final Text       widgetPassword;
    final StyledText widgetPublicKey;
    final StyledText widgetPrivateKey;
    final Spinner    widgetMaxConnectionCount;
    final Combo      widgetMaxStorageSize;
    final Button     widgetOK;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,1.0,1.0,0.0,0.0},new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(null,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetName = Widgets.newText(subComposite);
        widgetName.setText(serverData.name);
        Widgets.layout(widgetName,0,0,TableLayoutData.WE);

        widgetType = Widgets.newCombo(subComposite,SWT.READ_ONLY);
        Widgets.setComboItems(widgetType,
                              new Object[]{"file",  ServerTypes.FILE,
                                           "ftp",   ServerTypes.FTP,
                                           "ssh",   ServerTypes.SSH,
                                           "webdav",ServerTypes.WEBDAV
                                          }
                           );
        Widgets.setSelectedComboItem(widgetType,serverData.type);
        Widgets.layout(widgetType,0,1,TableLayoutData.E);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Login")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(null,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetLoginName = Widgets.newText(subComposite);
        widgetLoginName.setToolTipText(BARControl.tr("Login name."));
        widgetLoginName.setText(serverData.loginName);
        Widgets.layout(widgetLoginName,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(widgetLoginName,serverData)
        {
          public void modified(Control control)
          {
            Widgets.setEnabled(control,
                                  (serverData.type == ServerTypes.FTP)
                               || (serverData.type == ServerTypes.SSH)
                               || (serverData.type == ServerTypes.WEBDAV)
                              );
          }
        });

        label = Widgets.newLabel(subComposite,BARControl.tr("Port")+":");
        Widgets.layout(label,0,1,TableLayoutData.E);
        widgetPort = Widgets.newSpinner(subComposite);
        widgetPort.setToolTipText(BARControl.tr("Port number. Set to 0 to use default port number."));
        widgetPort.setMinimum(0);
        widgetPort.setMaximum(65535);
        widgetPort.setSelection(serverData.port);
        Widgets.layout(widgetPort,0,2,TableLayoutData.E,0,0,0,0,70,SWT.DEFAULT);
        Widgets.addModifyListener(new WidgetModifyListener(widgetPort,serverData)
        {
          public void modified(Control control)
          {
            Widgets.setEnabled(control,
                                  (serverData.type == ServerTypes.FTP)
                               || (serverData.type == ServerTypes.SSH)
                               || (serverData.type == ServerTypes.WEBDAV)
                              );
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Password")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      widgetPassword = Widgets.newPassword(composite);
      widgetPassword.setToolTipText(BARControl.tr("Set initial or new password. Note: existing password is not shown."));
      Widgets.layout(widgetPassword,2,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(widgetPassword,serverData)
      {
        public void modified(Control control)
        {
          Widgets.setEnabled(control,
                                (serverData.type == ServerTypes.FTP)
                             || (serverData.type == ServerTypes.SSH)
                             || (serverData.type == ServerTypes.WEBDAV)
                            );
        }
      });

      label = Widgets.newLabel(composite,BARControl.tr("Public key")+":");
      Widgets.layout(label,3,0,TableLayoutData.NW);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},new double[]{1.0,0.0}));
      Widgets.layout(subComposite,3,1,TableLayoutData.NSWE);
      {
        widgetPublicKey = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        widgetPublicKey.setToolTipText(BARControl.tr("Set initial or new public key. Note: existing public key is not shown."));
        Widgets.layout(widgetPublicKey,0,0,TableLayoutData.NSWE);
        Widgets.addModifyListener(new WidgetModifyListener(widgetPublicKey,serverData)
        {
          public void modified(Control control)
          {
            boolean enabled =    (serverData.type == ServerTypes.FTP)
                              || (serverData.type == ServerTypes.SSH)
                              || (serverData.type == ServerTypes.WEBDAV);
            control.setBackground(enabled ? null : COLOR_INACTIVE);
            Widgets.setEnabled(control,enabled);
          }
        });

        button = Widgets.newButton(subComposite,BARControl.tr("Load")+"\u2026");
        Widgets.layout(button,1,0,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String fileName = Dialogs.fileOpen(dialog,
                                               BARControl.tr("Select public key file"),
                                               lastKeyFileName,
                                               new String[]{BARControl.tr("Public keys"),"*.pub",
                                                            BARControl.tr("Public keys"),"*.public",
                                                            BARControl.tr("Key files"),"*.key",
                                                            BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                           },
                                               "*.pub"
                                              );
            if (fileName != null)
            {
              try
              {
                widgetPublicKey.setText(readKeyFile(fileName,widgetPublicKey.getLineDelimiter()));
                lastKeyFileName = fileName;
              }
              catch (IOException exception)
              {
                Dialogs.error(dialog,BARControl.tr("Cannot load public key file ''{0}'' (error: {1})",fileName,BARControl.reniceIOException(exception).getMessage()));
              }
            }
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,serverData)
        {
          public void modified(Control control)
          {
            Widgets.setEnabled(control,
                                  (serverData.type == ServerTypes.FTP)
                               || (serverData.type == ServerTypes.SSH)
                               || (serverData.type == ServerTypes.WEBDAV)
                              );
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Private key")+":");
      Widgets.layout(label,4,0,TableLayoutData.NW);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},new double[]{1.0,0.0}));
      Widgets.layout(subComposite,4,1,TableLayoutData.NSWE);
      {
        widgetPrivateKey = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        widgetPrivateKey.setToolTipText(BARControl.tr("Set initial or new private key. Note: existing private key is not shown."));
        Widgets.layout(widgetPrivateKey,0,0,TableLayoutData.NSWE);
        Widgets.addModifyListener(new WidgetModifyListener(widgetPrivateKey,serverData)
        {
          public void modified(Control control)
          {
            boolean enabled =    (serverData.type == ServerTypes.FTP)
                              || (serverData.type == ServerTypes.SSH)
                              || (serverData.type == ServerTypes.WEBDAV);
            control.setBackground(enabled ? null : COLOR_INACTIVE);
            Widgets.setEnabled(control,enabled);
          }
        });

        button = Widgets.newButton(subComposite,BARControl.tr("Load")+"\u2026");
        Widgets.layout(button,1,0,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String fileName = Dialogs.fileOpen(dialog,
                                               BARControl.tr("Select private key file"),
                                               lastKeyFileName,
                                               new String[]{BARControl.tr("Private keys"),"*.private",
                                                            BARControl.tr("Key files"),"*.key",
                                                            BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                           },
                                               "*"
                                              );
            if (fileName != null)
            {
              try
              {
                widgetPrivateKey.setText(readKeyFile(fileName,widgetPrivateKey.getLineDelimiter()));
                lastKeyFileName = fileName;
              }
              catch (IOException exception)
              {
                Dialogs.error(dialog,BARControl.tr("Cannot load private key file ''{0}'' (error: {1})",fileName,BARControl.reniceIOException(exception).getMessage()));
              }
            }
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,serverData)
        {
          public void modified(Control control)
          {
            Widgets.setEnabled(control,
                                  (serverData.type == ServerTypes.FTP)
                               || (serverData.type == ServerTypes.SSH)
                               || (serverData.type == ServerTypes.WEBDAV)
                              );
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Max. connections")+":");
      Widgets.layout(label,5,0,TableLayoutData.NW);
      widgetMaxConnectionCount = Widgets.newSpinner(composite);
      widgetMaxConnectionCount.setToolTipText(BARControl.tr("Max. number of concurrent connections. 0 for unlimited number of concurrent connections."));
      widgetMaxConnectionCount.setMinimum(0);
      widgetMaxConnectionCount.setSelection(serverData.maxConnectionCount);
      Widgets.layout(widgetMaxConnectionCount,5,1,TableLayoutData.W,0,0,0,0,70,SWT.DEFAULT);
      Widgets.addModifyListener(new WidgetModifyListener(widgetPort,serverData)
      {
        public void modified(Control control)
        {
          Widgets.setEnabled(control,
                                (serverData.type == ServerTypes.FTP)
                             || (serverData.type == ServerTypes.SSH)
                             || (serverData.type == ServerTypes.WEBDAV)
                            );
        }
      });

      label = Widgets.newLabel(composite,BARControl.tr("Max. storage size")+":");
      Widgets.layout(label,6,0,TableLayoutData.NW);
      widgetMaxStorageSize = Widgets.newCombo(composite);
      widgetMaxStorageSize.setToolTipText(BARControl.tr("Total size limit for storage."));
      widgetMaxStorageSize.setItems(new String[]{"32M","64M","128M","140M","256M","512M","1G","2G","4G","8G","64G","128G","512G","1T","2T","4T","8T"});
      widgetMaxStorageSize.setData("showedErrorDialog",false);
      widgetMaxStorageSize.setText(Units.formatByteSize(serverData.maxStorageSize));
      Widgets.layout(widgetMaxStorageSize,6,1,TableLayoutData.W);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetOK = Widgets.newButton(composite,okText);
      Widgets.layout(widgetOK,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetOK.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetType.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo widget = (Combo)selectionEvent.widget;

        serverData.type = Widgets.getSelectedComboItem(widget,ServerTypes.FILE);
Dprintf.dprintf("serverData.type=%s",serverData.type);
        Widgets.modified(serverData);
      }
    });
    widgetOK.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget  = (Button)selectionEvent.widget;

        serverData.name               = widgetName.getText().trim();
        serverData.loginName          = widgetLoginName.getText();
        serverData.port               = widgetPort.getSelection();
        serverData.password           = widgetPassword.getText();
        serverData.publicKey          = widgetPublicKey.getText().replace(widgetPublicKey.getLineDelimiter(),"\n").trim();
        serverData.privateKey         = widgetPrivateKey.getText().replace(widgetPublicKey.getLineDelimiter(),"\n").trim();
        serverData.maxConnectionCount = widgetMaxConnectionCount.getSelection();
        serverData.maxStorageSize     = Units.parseByteSize(widgetMaxStorageSize.getText());

        Dialogs.close(dialog,true);
      }
    });

    widgetName.forceFocus();

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** read key file
   * @param fileName file name
   * @param lineDelimiter line delimiter
   * @return key data
   */
  private static String readKeyFile(String fileName, String lineDelimiter)
    throws IOException
  {
    StringBuilder buffer = new StringBuilder();

    BufferedReader input = null;
    try
    {
      // open file
      input = new BufferedReader(new FileReader(fileName));

      // read file
      String   line;
      while ((line = input.readLine()) != null)
      {
        line = line.trim();

        buffer.append(line+lineDelimiter);
      }

      // close file
      input.close(); input = null;
    }
    finally
    {
      try
      {
        if (input != null) input.close();
      }
      catch (IOException exception)
      {
        // ignored
      }
    }

    return buffer.toString();
  }
}

/* end of file */
