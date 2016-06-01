/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Settings.java,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: settings load/save functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Field;
import java.lang.reflect.Constructor;
import java.lang.reflect.ParameterizedType;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.TYPE;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;

/****************************** Classes ********************************/

/** setting comment annotation
 */
@Target({TYPE,FIELD})
@Retention(RetentionPolicy.RUNTIME)
@interface SettingComment
{
  String[] text() default {""};                  // comment before value
}

/** setting value migrate interface
 */
interface SettingMigrate
{
  /** migrate value
   * @param value current value
   * @return new value
   */
  public Object run(Object value);
}

/** setting value annotation
 */
@Target({TYPE,FIELD})
@Retention(RetentionPolicy.RUNTIME)
@interface SettingValue
{
  String  name()         default "";              // name of value
  String  defaultValue() default "";              // default value
  Class   type()         default DEFAULT.class;   // adapter class
  boolean obsolete()     default false;           // true iff obsolete setting

  Class migrate() default DEFAULT.class;

  static final class DEFAULT
  {
  }
}

/** setting value adapter
 */
abstract class SettingValueAdapter<String,Value>
{
  /** convert to value
   * @param string string
   * @return value
   */
  abstract public Value toValue(String string)throws Exception;

  /** convert to string
   * @param value value
   * @return string
   */
  abstract public String toString(Value value) throws Exception;

  /** check if equals
   * @param value0,value1 values to compare
   * @return true if value0==value1
   */
  public boolean equals(Value value0, Value value1)
  {
    return false;
  }

  /** migrate values
   */
  public void Xmigrate(Object value)
  {
  }
}

/** setting utils
 */
public class SettingUtils
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private static long lastModified = 0L;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** load program settings
   * @param file settings file to load
   */
  public static void load(File file)
  {
    if (file.exists())
    {
      // get setting classes
      Class[] settingClasses = getSettingClasses();

      BufferedReader input = null;
      try
      {
        // open file
        input = new BufferedReader(new FileReader(file));

        // read file
        int      lineNb = 0;
        String   line;
        Object[] data = new Object[2];
        while ((line = input.readLine()) != null)
        {
          line = line.trim();
          lineNb++;

          // check comment
          if (line.isEmpty() || line.startsWith("#"))
          {
            continue;
          }

          // parse
          if (StringParser.parse(line,"%s = % s",data))
          {
            String name   = (String)data[0];
            String string = (String)data[1];

            for (Class clazz : settingClasses)
            {
              for (Field field : clazz.getDeclaredFields())
              {
                for (Annotation annotation : field.getDeclaredAnnotations())
                {
                  if (annotation instanceof SettingValue)
                  {
                    SettingValue settingValue = (SettingValue)annotation;

                    if (((!settingValue.name().isEmpty()) ? settingValue.name() : field.getName()).equals(name))
                    {
                      try
                      {
                        Class type = field.getType();
                        if      (type.isArray())
                        {
                          // array type
                          type = type.getComponentType();
                          if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            field.set(null,addArrayUniq((Object[])field.get(null),value,settingValueAdapter));
                          }
                          else if (type == int.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,addArrayUniq((int[])field.get(null),value));
                          }
                          else if (type == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,addArrayUniq((Integer[])field.get(null),value));
                          }
                          else if (type == long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,addArrayUniq((long[])field.get(null),value));
                          }
                          else if (type == Long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,addArrayUniq((Long[])field.get(null),value));
                          }
                          else if (type == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,addArrayUniq((boolean[])field.get(null),value));
                          }
                          else if (type == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,addArrayUniq((Boolean[])field.get(null),value));
                          }
                          else if (type == String.class)
                          {
                            field.set(null,addArrayUniq((String[])field.get(null),StringUtils.unescape(string)));
                          }
                          else if (type.isEnum())
                          {
                            field.set(null,addArrayUniq((Enum[])field.get(null),StringUtils.parseEnum(type,string)));
                          }
                          else if (type == EnumSet.class)
                          {
                            field.set(null,addArrayUniq((EnumSet[])field.get(null),StringUtils.parseEnumSet(type,string)));
                          }
                          else
                          {
Dprintf.dprintf("field.getType()=%s",type);
                          }
                        }
                        else if (Set.class.isAssignableFrom(type))
                        {
                          // Set type
                          Class<?> setType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                          if (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == int.class)
                          {
                            int value = Integer.parseInt(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == long.class)
                          {
                            long value = Long.parseLong(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == Long.class)
                          {
                            long value = Long.parseLong(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (setType == String.class)
                          {
                            ((Set)field.get(null)).add(StringUtils.unescape(string));
                          }
                          else if (setType.isEnum())
                          {
                            ((Set)field.get(null)).add(StringUtils.parseEnum(type,string));
                          }
                          else if (setType == EnumSet.class)
                          {
                            ((Set)field.get(null)).add(StringUtils.parseEnumSet(type,string));
                          }
                          else
                          {
                            throw new Error(String.format("%s: Set without value adapter %s",field,setType));
                          }
                        }
                        else if (List.class.isAssignableFrom(type))
                        {
                          // List type
                          Class<?> listType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                          if (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == int.class)
                          {
                            int value = Integer.parseInt(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == long.class)
                          {
                            long value = Long.parseLong(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == Long.class)
                          {
                            long value = Long.parseLong(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (listType == String.class)
                          {
                            ((List)field.get(null)).add(StringUtils.unescape(string));
                          }
                          else if (listType.isEnum())
                          {
                            ((List)field.get(null)).add(StringUtils.parseEnum(type,string));
                          }
                          else if (listType == EnumSet.class)
                          {
                            ((List)field.get(null)).add(StringUtils.parseEnumSet(type,string));
                          }
                          else
                          {
                            throw new Error(String.format("%s: List without value adapter %s",field,listType));
                          }
                        }
                        else
                        {
                          // primitiv type
                          if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            field.set(null,value);
                          }
                          else if (type == int.class)
                          {
                            int value = Integer.parseInt(string);
                            field.setInt(null,value);
                          }
                          else if (type == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,new Integer(value));
                          }
                          else if (type == long.class)
                          {
                            long value = Long.parseLong(string);
                            field.setLong(null,value);
                          }
                          else if (type == Long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,new Long(value));
                          }
                          else if (type == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.setBoolean(null,value);
                          }
                          else if (type == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,new Boolean(value));
                          }
                          else if (type == String.class)
                          {
                            field.set(null,StringUtils.unescape(string));
                          }
                          else if (type.isEnum())
                          {
                            field.set(null,StringUtils.parseEnum(type,string));
                          }
                          else if (type == EnumSet.class)
                          {
                            Class enumClass = settingValue.type();
                            if (!enumClass.isEnum())
                            {
                              throw new Error(enumClass+" is not an enum class!");
                            }
                            field.set(null,StringUtils.parseEnumSet(enumClass,string));
                          }
                          else
                          {
Dprintf.dprintf("field.getType()=%s",type);
                          }
                        }
                      }
                      catch (NumberFormatException exception)
                      {
                        BARControl.printWarning("Cannot parse number '%s' for configuration value '%s' in line %d",string,name,lineNb);
                      }
                      catch (Exception exception)
                      {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
                      }
                    }
                  }
                  else
                  {
                  }
                }
              }
            }
          }
          else
          {
            BARControl.printWarning("Unknown configuration value '%s' in line %d",line,lineNb);
          }
        }

        // close file
        input.close(); input = null;
      }
      catch (IOException exception)
      {
        // ignored
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

      // migrate values
      for (Class clazz : settingClasses)
      {
        for (Field field : clazz.getDeclaredFields())
        {
          for (Annotation annotation : field.getDeclaredAnnotations())
          {
            if (annotation instanceof SettingValue)
            {
              SettingValue settingValue = (SettingValue)annotation;

              try
              {
                Class migrate = settingValue.migrate();
                if (migrate != SettingValue.DEFAULT.class)
                {
                  Constructor constructor = migrate.getDeclaredConstructor(Settings.class);
                  SettingMigrate settingMigrate = (SettingMigrate)constructor.newInstance(new Settings());
//                  settingMigrate.run(settingValue);
                  field.set(null,settingMigrate.run(field.get(null)));
                }
              }
              catch (Exception exception)
              {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
              }

            }
          }
        }
      }
    }

    // save last modified time
    lastModified = file.lastModified();
  }

  /** save program settings
   * @param fileName file nam
   */
  public static void save(File file)
  {
    // create directory
    File directory = file.getParentFile();
    if ((directory != null) && !directory.exists()) directory.mkdirs();

    PrintWriter output = null;
    try
    {
      // get setting classes
      Class[] settingClasses = getSettingClasses();

      // open file
      output = new PrintWriter(new FileWriter(file));

      // write settings
      for (Class clazz : settingClasses)
      {
        for (Field field : clazz.getDeclaredFields())
        {
//Dprintf.dprintf("field=%s",field);
          for (Annotation annotation : field.getDeclaredAnnotations())
          {
            if      (annotation instanceof SettingValue)
            {
              SettingValue settingValue = (SettingValue)annotation;

              if (!settingValue.obsolete())
              {
              // get value and write to file
              String name = (!settingValue.name().isEmpty()) ? settingValue.name() : field.getName();
              try
              {
                Class type = field.getType();
                if      (type.isArray())
                {
                  // array type
                  type = type.getComponentType();
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (Object[])field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (type == int.class)
                  {
                    for (int value : (int[])field.get(null))
                    {
                      output.printf("%s = %d\n",name,value);
                    }
                  }
                  else if (type == Integer.class)
                  {
                    for (int value : (Integer[])field.get(null))
                    {
                      output.printf("%s = %d\n",name,value);
                    }
                  }
                  else if (type == long.class)
                  {
                    for (long value : (long[])field.get(null))
                    {
                      output.printf("%s = %ld\n",name,value);
                    }
                  }
                  else if (type == Long.class)
                  {
                    for (long value : (Long[])field.get(null))
                    {
                      output.printf("%s = %ld\n",name,value);
                    }
                  }
                  else if (type == boolean.class)
                  {
                    for (boolean value : (boolean[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value ? "yes" : "no");
                    }
                  }
                  else if (type == Boolean.class)
                  {
                    for (boolean value : (Boolean[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value ? "yes" : "no");
                    }
                  }
                  else if (type == String.class)
                  {
                    for (String value : (String[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape(value));
                    }
                  }
                  else if (type.isEnum())
                  {
                    for (Enum value : (Enum[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value.toString());
                    }
                  }
                  else if (type == EnumSet.class)
                  {
                    for (EnumSet enumSet : (EnumSet[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                    }
                  }
                  else
                  {
Dprintf.dprintf("field.getType()=%s",type);
                  }
                }
                else if (Set.class.isAssignableFrom(type))
                {
                  // Set type
                  Class<?> setType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (Set)field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (setType == int.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (setType == Integer.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (setType == long.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (setType == Long.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (setType == boolean.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (setType == Boolean.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (setType == String.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape((String)object));
                    }
                  }
                  else if (settingValue.type().isEnum())
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,((Enum)object).toString());
                    }
                  }
                  else if (setType == EnumSet.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((EnumSet)object,","));
                    }
                  }
                  else
                  {
                    throw new Error(String.format("%s: Set without value adapter %s",field,setType));
                  }
                }
                else if (List.class.isAssignableFrom(type))
                {
                  // List type
                  Class<?> listType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (List)field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (listType == int.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (listType == Integer.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (listType == long.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (listType == Long.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (listType == boolean.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (listType == Boolean.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (listType == String.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape((String)object));
                    }
                  }
                  else if (listType.isEnum())
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,((Enum)object).toString());
                    }
                  }
                  else if (listType == EnumSet.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((EnumSet)object,","));
                    }
                  }
                  else
                  {
                    throw new Error(String.format("%s: List without value adapter %s",field,listType));
                  }
                }
                else
                {
                  // primitiv type
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    String value = (String)settingValueAdapter.toString(field.get(null));
                    output.printf("%s = %s\n",name,value);
                  }
                  else if (type == int.class)
                  {
                    int value = field.getInt(null);
                    output.printf("%s = %d\n",name,value);
                  }
                  else if (type == Integer.class)
                  {
                    int value = (Integer)field.get(null);
                    output.printf("%s = %d\n",name,value);
                  }
                  else if (type == long.class)
                  {
                    long value = field.getLong(null);
                    output.printf("%s = %ld\n",name,value);
                  }
                  else if (type == Long.class)
                  {
                    long value = (Long)field.get(null);
                    output.printf("%s = %ld\n",name,value);
                  }
                  else if (type == boolean.class)
                  {
                    boolean value = field.getBoolean(null);
                    output.printf("%s = %s\n",name,value ? "yes" : "no");
                  }
                  else if (type == Boolean.class)
                  {
                    boolean value = (Boolean)field.get(null);
                    output.printf("%s = %s\n",name,value ? "yes" : "no");
                  }
                  else if (type == String.class)
                  {
                    String value = (type != null) ? (String)field.get(null) : settingValue.defaultValue();
                    output.printf("%s = %s\n",name,StringUtils.escape(value));
                  }
                  else if (type.isEnum())
                  {
                    Enum value = (Enum)field.get(null);
                    output.printf("%s = %s\n",name,value.toString());
                  }
                  else if (type == EnumSet.class)
                  {
                    EnumSet enumSet = (EnumSet)field.get(null);
                    output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                  }
                  else
                  {
Dprintf.dprintf("field.getType()=%s",type);
                  }
                }
              }
              catch (Exception exception)
              {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
              }
              }
            }
            else if (annotation instanceof SettingComment)
            {
              SettingComment settingComment = (SettingComment)annotation;

              for (String line : settingComment.text())
              {
                if (!line.isEmpty())
                {
                  output.printf("# %s\n",line);
                }
                else
                {
                  output.printf("\n");
                }
              }
            }
          }
        }
      }

      // close file
      output.close();

      // save last modified time
      lastModified = file.lastModified();
    }
    catch (IOException exception)
    {
      // ignored
    }
    finally
    {
      if (output != null) output.close();
    }
  }

  /** check if program settings file is modified
   * @param file file
   * @return true iff modified
   */
  public static boolean isModified(File file)
  {
    return (lastModified != 0L) && (file.lastModified() > lastModified);
  }

  //-----------------------------------------------------------------------

  /** get all setting classes
   * @return classes array
   */
  protected static Class[] getSettingClasses()
  {
    // get all setting classes
    ArrayList<Class> classList = new ArrayList<Class>();

    classList.add(Settings.class);
    for (Class clazz : Settings.class.getDeclaredClasses())
    {
//Dprintf.dprintf("c=%s",clazz);
      classList.add(clazz);
    }

    return classList.toArray(new Class[classList.size()]);
  }

  /** unique add element to int array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static int[] addArrayUniq(int[] array, int n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to int array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Integer[] addArrayUniq(Integer[] array, int n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static long[] addArrayUniq(long[] array, long n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Long[] addArrayUniq(Long[] array, long n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static boolean[] addArrayUniq(boolean[] array, boolean n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Boolean[] addArrayUniq(Boolean[] array, boolean n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to string array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static String[] addArrayUniq(String[] array, String string)
  {
    int z = 0;
    while ((z < array.length) && !array[z].equals(string))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = string;
    }

    return array;
  }

  /** unique add element to enum array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static Enum[] addArrayUniq(Enum[] array, Enum n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to enum set array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static EnumSet[] addArrayUniq(EnumSet[] array, EnumSet n)
  {
    int z = 0;
    while ((z < array.length) && (array[z].equals(n)))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to object array
   * @param array array
   * @param object element
   * @param settingAdapter setting adapter (use equals() function)
   * @return extended array or array
   */
  private static Object[] addArrayUniq(Object[] array, Object object, SettingValueAdapter settingValueAdapter)
  {
    int z = 0;
    while ((z < array.length) && !settingValueAdapter.equals(array[z],object))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = object;
    }

    return array;
  }
}

/* end of file */
