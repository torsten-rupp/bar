/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
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
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Constructor;
import java.lang.reflect.Modifier;
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
import java.util.Collection;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

// graphics
import org.eclipse.swt.widgets.Display;

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
  String   name()            default "";              // name of value
  String   defaultValue()    default "";              // default value
  Class    type()            default DEFAULT.class;   // adapter class
  boolean  unique()          default false;           // true iff unique value
  boolean  deprecated()      default false;           // true iff deprecated setting
  String[] deprecatedNames() default "";              // deprecated names

  Class migrate() default DEFAULT.class;

  static final class DEFAULT
  {
  }
}

/** setting section annotation
 */
@Target({TYPE,FIELD})
@Retention(RetentionPolicy.RUNTIME)
@interface SettingSection
{
  String name() default "";                      // section name
  String value() default "";                     // section value
}

/** setting utils
 */
public class SettingUtils
{
  /** array list
   */
  static class ArrayList<T> extends java.util.ArrayList<T>
  {
    ArrayList(T... defaultValues)
    {
      for (T value : defaultValues)
      {
        add(value);
      }
      SettingUtils.setModified();
    }

    @Override
    public boolean add(T value)
    {
      boolean result = super.add(value);
      SettingUtils.setModified();

      return result;
    }

    @Override
    public boolean addAll(Collection<? extends T> values)
    {
      boolean result = super.addAll(values);
      SettingUtils.setModified();

      return result;
    }

    @Override
    public boolean remove(Object value)
    {
      boolean result = super.remove(value);
      SettingUtils.setModified();

      return result;
    }
  }

  /** hash set
   */
  static class HashSet<T> extends java.util.HashSet<T>
  {
    HashSet(T... defaultValues)
    {
      for (T value : defaultValues)
      {
        add(value);
      }
    }

    @Override
    public boolean add(T value)
    {
      boolean result = super.add(value);
      SettingUtils.setModified();

      return result;
    }

    @Override
    public boolean addAll(Collection<? extends T> values)
    {
      boolean result = super.addAll(values);
      SettingUtils.setModified();

      return result;
    }

    @Override
    public boolean remove(Object value)
    {
      boolean result = super.remove(value);
      SettingUtils.setModified();

      return result;
    }
  }

  /** hash map
   */
  static class HashMap<K,T> extends java.util.HashMap<K,T>
  {
    HashMap(Object... defaultValues)
    {
      for (int i = 0; i < defaultValues.length; i += 2)
      {
        put((K)defaultValues[i+0],(T)defaultValues[i+1]);
      }
    }

    @Override
    public T put(K key, T value)
    {
      T result = super.put(key,value);
      SettingUtils.setModified();

      return result;
    }

    @Override
    public void putAll(Map<? extends K, ? extends T> values)
    {
      super.putAll(values);
      SettingUtils.setModified();
    }

    @Override
    public T remove(Object value)
    {
      T result = super.remove(value);
      SettingUtils.setModified();

      return result;
    }
  }

  /** value set
   */
  static class ValueMap<T> extends HashMap<String,T>
  {
    /** create value set
     * @param defaultValue default value
     */
    ValueMap(T defaultValue)
    {
      super();
      super.put("",defaultValue);
    }

    /** check if value exists
     * @param name name
     * @return true iff value exists
     */
    public boolean contains(String name)
    {
      return super.containsKey(name);
    }

    /** get value
     * @return value
     */
    public T get()
    {
      return super.get("");
    }

    /** set value
     * @param value value
     */
    public void set(T value)
    {
      super.put("",value);
    }

    /** get value
     * @param name name
     * @return value or default value
     */
    public T get(String name)
    {
      return contains(name) ? super.get(name) : super.get("");
    }

    /** set value
     * @param name name
     * @param value value
     */
    public void set(String name, T value)
    {
      super.put(name,value);
    }

    /** get sorted names including default
     * @return names
     */
    public String[] names()
    {
      HashSet<String> nameSet = new HashSet<String>();
      for (String key : keySet())
      {
        nameSet.add(key);
      }

      String[] names = nameSet.toArray(new String[nameSet.size()]);
      Arrays.sort(names);

      return names;
    }

    /** get default value as string
     * @return string
     */
    public String toString()
    {
      return get("").toString();
    }
  }

  /** value adapter
   */
  abstract static class ValueAdapter<String,Value>
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
    public void migrate(Object value)
    {
    }
  }

  /** simple integer array
   */
  static class SimpleIntegerArray
  {
    public final int[] intArray;

    /** create simple integer array
     * @param int integer array
     */
    SimpleIntegerArray(int[] intArray)
    {
      this.intArray = intArray;
    }

    /** create simple integer array
     */
    SimpleIntegerArray()
    {
      this.intArray = null;
    }

    /** create simple integer array
     * @param intList integer list
     */
    SimpleIntegerArray(ArrayList<Integer> intList)
    {
      this.intArray = new int[intList.size()];
      for (int i = 0; i < intList.size(); i++)
      {
        this.intArray[i] = intList.get(i);
      }
    }

    /** get integer
     * @param index index (0..n-1)
     * @return integer
     */
    public int get(int index)
    {
      return (index < intArray.length) ? intArray[index] : null;
    }

    /** get int array
     * @return int array
     */
    public int[] get()
    {
      return intArray;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (int i : intArray)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Integer.toString(i));
      }
      return "Integers {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> integer array
   */
  static class ValueAdapterSimpleIntegerArray extends ValueAdapter<String,SimpleIntegerArray>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public SimpleIntegerArray toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<Integer> integerList = new ArrayList<Integer>();
      while (tokenizer.hasMoreTokens())
      {
        integerList.add(Integer.parseInt(tokenizer.nextToken()));
      }
      return new SimpleIntegerArray(integerList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(SimpleIntegerArray simpleIntegerArray) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (int i : simpleIntegerArray.get())
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Integer.toString(i));
      }
      return buffer.toString();
    }
  }

  /** long array
   */
  static class SimpleLongArray
  {
    public final long[] longArray;

    /** create simple long array
     * @param longArray long array
     */
    SimpleLongArray(long[] longArray)
    {
      this.longArray = longArray;
    }

    /** create simple long array
     */
    SimpleLongArray()
    {
      this.longArray = null;
    }

    /** create simple long array
     * @param widthList with list
     */
    SimpleLongArray(ArrayList<Long> longList)
    {
      this.longArray = new long[longList.size()];
      for (int i = 0; i < longList.size(); i++)
      {
        this.longArray[i] = longList.get(i);
      }
    }

    /** get long
     * @param index index (0..n-1)
     * @return long
     */
    public long get(int index)
    {
      return (index < longArray.length) ? longArray[index] : null;
    }

    /** get long array
     * @return long array
     */
    public long[] get()
    {
      return longArray;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (long l : longArray)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Long.toString(l));
      }
      return "Longs {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> long array
   */
  static class ValueAdapterSimpleLongArray extends ValueAdapter<String,SimpleLongArray>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public SimpleLongArray toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<Long> longList = new ArrayList<Long>();
      while (tokenizer.hasMoreTokens())
      {
        longList.add(Long.parseLong(tokenizer.nextToken()));
      }
      return new SimpleLongArray(longList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(SimpleLongArray simpleLongArray) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (long l : simpleLongArray.get())
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Long.toString(l));
      }
      return buffer.toString();
    }
  }

  /** double array
   */
  static class SimpleDoubleArray
  {
    public final double[] doubleArray;

    /** create simple double array
     * @param doubleArray double array
     */
    SimpleDoubleArray(double[] doubleArray)
    {
      this.doubleArray = doubleArray;
    }

    /** create simple double array
     */
    SimpleDoubleArray()
    {
      this.doubleArray = null;
    }

    /** create simple double array
     * @param widthList with list
     */
    SimpleDoubleArray(ArrayList<Double> doubleList)
    {
      this.doubleArray = new double[doubleList.size()];
      for (int i = 0; i < doubleList.size(); i++)
      {
        this.doubleArray[i] = doubleList.get(i);
      }
    }

    /** get double
     * @param index index (0..n-1)
     * @return string
     */
    public double get(int index)
    {
      return (index < doubleArray.length) ? doubleArray[index] : null;
    }

    /** get double array
     * @return double array
     */
    public double[] get()
    {
      return doubleArray;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (double d : doubleArray)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Double.toString(d));
      }
      return "Doubles {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> string array
   */
  static class ValueAdapterSimpleDoubleArray extends ValueAdapter<String,SimpleDoubleArray>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public SimpleDoubleArray toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<Double> doubleList = new ArrayList<Double>();
      while (tokenizer.hasMoreTokens())
      {
        doubleList.add(Double.parseDouble(tokenizer.nextToken()));
      }
      return new SimpleDoubleArray(doubleList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(SimpleDoubleArray simpleDoubleArray) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (double d : simpleDoubleArray.get())
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(String.format("%.4f",d));
      }
      return buffer.toString();
    }
  }

  /** simple string array
   */
  static class SimpleStringArray
  {
    public final String[] stringArray;

    /** create simple string array
     * @param width width array
     */
    SimpleStringArray(String[] stringArray)
    {
      this.stringArray = stringArray;
    }

    /** create simple string array
     */
    SimpleStringArray()
    {
      this((String[])null);
    }

    /** create simple string array
     * @param widthList with list
     */
    SimpleStringArray(ArrayList<String> stringList)
    {
      this.stringArray = stringList.toArray(new String[stringList.size()]);
    }

    /** get string
     * @param index index (0..n-1)
     * @return string or null
     */
    public String get(int index)
    {
      return (index < stringArray.length) ? stringArray[index] : null;
    }

    /** get string array
     * @return string array
     */
    public String[] get()
    {
      return stringArray;
    }

    /** get mapped indices
     * @return indices
     */
    public int[] getMap(String strings[])
    {
      int indices[] = new int[strings.length];
      for (int i = 0; i < strings.length; i++)
      {
        indices[i] = i;
      }
      if (stringArray != null)
      {
        for (int i = 0; i < stringArray.length; i++)
        {
          int j = StringUtils.indexOf(strings,stringArray[i]);
          if ((j >= i) && (j < stringArray.length))
          {
            int n = indices[i];
            indices[i] = indices[j];
            indices[j] = n;
          }
        }
      }

      return indices;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (String string : stringArray)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(string);
      }
      return "Strings {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> string array
   */
  static class ValueAdapterSimpleStringArray extends ValueAdapter<String,SimpleStringArray>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public SimpleStringArray toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<String> stringList = new ArrayList<String>();
      while (tokenizer.hasMoreTokens())
      {
        stringList.add(tokenizer.nextToken());
      }
      return new SimpleStringArray(stringList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(SimpleStringArray simpleStringArray) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      String strings[] = simpleStringArray.get();
      if (strings != null)
      {
        for (String string : strings)
        {
          if (buffer.length() > 0) buffer.append(',');
          buffer.append((string != null) ? string : "");
        }
      }
      return buffer.toString();
    }
  }

  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private static boolean permitModifyFlag     = false;
  private static boolean modifiedFlag         = false;
  private static long    lastExternalModified = 0L;

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

     // collect field markers "has default value"
     HashSet<Field> hasDefaultFields = new HashSet<Field>();
     for (Class clazz : settingClasses)
     {
       for (Field field : clazz.getDeclaredFields())
       {
         hasDefaultFields.add(field);
       }
     }

      BufferedReader input = null;
      try
      {
        // open file
        input = new BufferedReader(new FileReader(file));

        // read file
        int      lineNb = 0;
        String   line;
        Object[] data = new Object[3];
        String   name;
        String   valueMapName;
        String   string;
        while ((line = input.readLine()) != null)
        {
          line = line.trim();
          lineNb++;
//Dprintf.dprintf("line=%s",line);

          // check comment
          if (line.isEmpty() || line.startsWith("#"))
          {
            continue;
          }

          // parse
          if      (StringParser.parse(line,"%s[% s]=% s",data))
          {
            name         = (String)data[0];
            valueMapName = (String)data[1];
            string       = (String)data[2];
          }
          else if (StringParser.parse(line,"%s=% s",data))
          {
            name         = (String)data[0];
            valueMapName = null;
            string       = (String)data[1];
          }
          else
          {
            System.err.println(String.format("Unknown configuration value '%s' in line %d - skipped",line,lineNb));
            continue;
          }

          // store value
          for (Class clazz : settingClasses)
          {
            for (Field field : clazz.getDeclaredFields())
            {
              for (Annotation annotation : field.getDeclaredAnnotations())
              {
                if (annotation instanceof SettingValue)
                {
                  SettingValue settingValue = (SettingValue)annotation;
                    if (   ((!settingValue.name().isEmpty()) ? settingValue.name() : field.getName()).equals(name)
                        || nameEquals(settingValue.deprecatedNames(),name)
                       )
                  {
                    try
                    {
                      Class type = field.getType();
                      if      (type.isArray())
                      {
                        // array type
                        type = type.getComponentType();
                        if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                        {
                          // instantiate config adapter class
                          ValueAdapter settingValueAdapter;
                          Class enclosingClass = settingValue.type().getEnclosingClass();
                          if (   (enclosingClass == Settings.class)
                              && settingValue.type().isMemberClass()
                              && !Modifier.isStatic(settingValue.type().getModifiers())
                             )
                          {
                            Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                            settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                          }
                          else
                          {
                            settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                          }

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,Array.newInstance(type,0));
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          Object value = settingValueAdapter.toValue(string);
                          if (value != null) field.set(null,addArrayUniq((Object[])field.get(null),value,settingValueAdapter));
                        }
                        else if (type == int.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new int[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          field.set(null,addArrayUniq((int[])field.get(null),value));
                        }
                        else if (type == Integer.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new Integer[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          field.set(null,addArrayUniq((Integer[])field.get(null),value));
                        }
                        else if (type == long.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new long[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          long value = Long.parseLong(string);
                          field.set(null,addArrayUniq((long[])field.get(null),value));
                        }
                        else if (type == Long.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new Long[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          long value = Long.parseLong(string);
                          field.set(null,addArrayUniq((Long[])field.get(null),value));
                        }
                        else if (type == double.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new double[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          double value = Double.parseDouble(string);
                          field.set(null,addArrayUniq((double[])field.get(null),value));
                        }
                        else if (type == Double.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new Double[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          double value = Double.parseDouble(string);
                          field.set(null,addArrayUniq((Double[])field.get(null),value));
                        }
                        else if (type == boolean.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new boolean[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          boolean value = StringUtils.parseBoolean(string);
                          field.set(null,addArrayUniq((boolean[])field.get(null),value));
                        }
                        else if (type == Boolean.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new Boolean[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          boolean value = StringUtils.parseBoolean(string);
                          field.set(null,addArrayUniq((Boolean[])field.get(null),value));
                        }
                        else if (type == String.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new String[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          field.set(null,addArray((String[])field.get(null),StringUtils.unescape(string)));
                        }
                        else if (type == EnumSet.class)
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new EnumSet[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          field.set(null,addArrayUniq((EnumSet[])field.get(null),StringUtils.parseEnumSet(type,string)));
                        }
                        else if (type.isEnum())
                        {
                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            field.set(null,new Enum[0]);
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          field.set(null,addArrayUniq((Enum[])field.get(null),StringUtils.parseEnum(type,string)));
                        }
                        else
                        {
//TODO: error message?
//Dprintf.dprintf("field.getType()=%s",type);
                        }
                      }
                      else if (type == HashSet.class)
                      {
                        Class<?> setType = (Class)(((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0]);
                        if (setType != null)
                        {
                          if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            ValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (   (enclosingClass == Settings.class)
                                && settingValue.type().isMemberClass()
                                && !Modifier.isStatic(settingValue.type().getModifiers())
                               )
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                            }

                            // get value
                            HashSet<Object> hashSet = (HashSet<Object>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            Object value = settingValueAdapter.toValue(string);
                            if (value != null) hashSet.add(value);
                          }
                          else if (setType == Integer.class)
                          {
                            // get value
                            HashSet<Integer> hashSet = (HashSet<Integer>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            int value = Integer.parseInt(string);
                            hashSet.add(value);
                          }
                          else if (setType == Long.class)
                          {
                            // get value
                            HashSet<Long> hashSet = (HashSet<Long>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            long value = Long.parseLong(string);
                            hashSet.add(value);
                          }
                          else if (setType == Boolean.class)
                          {
                            // get value
                            HashSet<Boolean> hashSet = (HashSet<Boolean>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            boolean value = StringUtils.parseBoolean(string);
                            hashSet.add(value);
                          }
                          else if (setType == String.class)
                          {
                            // get value
                            HashSet<String> hashSet = (HashSet<String>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            String value = StringUtils.unescape(string);
                            hashSet.add(value);
                          }
                          else if (setType.isEnum())
                          {
                            // get value
                            HashSet<Enum> hashSet = (HashSet<Enum>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            Enum value = StringUtils.parseEnum(type,string);
                            hashSet.add(value);
                          }
                          else if (setType == EnumSet.class)
                          {
                            // get value
                            HashSet<EnumSet> hashSet = (HashSet<EnumSet>)field.get(null);

                            // clear default value
                            if (hasDefaultFields.contains(field))
                            {
                              hashSet.clear();
                              hasDefaultFields.remove(field);
                            }

                            // set value
                            EnumSet value = StringUtils.parseEnumSet(type,string);
                            hashSet.add(value);
                          }
                        }
                        else
                        {
                          throw new Error(String.format("Hash set '%s' without type",field.getName()));
                        }
                      }
                      else if (type == ValueMap.class)
                      {
                        // get value
                        ValueMap valueMap = (ValueMap)field.get(null);

                        // clear default value
                        if (hasDefaultFields.contains(field))
                        {
                          valueMap.clear();
                          hasDefaultFields.remove(field);
                        }

                        // set value
                        String value = StringUtils.unescape(string);
                        if (valueMapName != null)
                        {
                          valueMap.put(valueMapName,value);
                        }
                        else
                        {
                          valueMap.set(value);
                        }
                      }
                      else if (Set.class.isAssignableFrom(type))
                      {
                        // Set type
                        Class<?> setType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                        if (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                        {
                          // instantiate config adapter class
                          ValueAdapter settingValueAdapter;
                          Class enclosingClass = settingValue.type().getEnclosingClass();
                          if (   (enclosingClass == Settings.class)
                              && settingValue.type().isMemberClass()
                              && !Modifier.isStatic(settingValue.type().getModifiers())
                             )
                          {
                            Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                            settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                          }
                          else
                          {
                            settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                          }

                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          Object value = settingValueAdapter.toValue(string);
                          if (value != null) set.add(value);
                        }
                        else if (setType == int.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          set.add(value);
                        }
                        else if (setType == Integer.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          set.add(value);
                        }
                        else if (setType == long.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          long value = Long.parseLong(string);
                          set.add(value);
                        }
                        else if (setType == Long.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          long value = Long.parseLong(string);
                          set.add(value);
                        }
                        else if (setType == double.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          double value = Double.parseDouble(string);
                          set.add(value);
                        }
                        else if (setType == Long.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          double value = Double.parseDouble(string);
                          set.add(value);
                        }
                        else if (setType == boolean.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          boolean value = StringUtils.parseBoolean(string);
                          set.add(value);
                        }
                        else if (setType == Boolean.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          boolean value = StringUtils.parseBoolean(string);
                          set.add(value);
                        }
                        else if (setType == String.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          set.add(StringUtils.unescape(string));
                        }
                        else if (type == EnumSet.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          Iterator iterator = StringUtils.parseEnumSet(setType,string).iterator();
                          while (iterator.hasNext())
                          {
                            set.add(iterator.next());
                          }
                        }
                        else if (setType.isEnum())
                        {
//Dprintf.dprintf("type=%s",type);
//Dprintf.dprintf("setType=%s",setType);
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          set.add(StringUtils.parseEnum(setType,string));
                        }
                        else
                        {
                          throw new Error(String.format("Set '%s' without value adapter %s",field.getName(),setType));
                        }
                      }
                      else if (List.class.isAssignableFrom(type))
                      {
                        // List type
                        Class<?> listType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                        if (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                        {
                          // instantiate config adapter class
                          ValueAdapter settingValueAdapter;
                          Class enclosingClass = settingValue.type().getEnclosingClass();
                          if (   (enclosingClass == Settings.class)
                              && settingValue.type().isMemberClass()
                              && !Modifier.isStatic(settingValue.type().getModifiers())
                             )
                          {
                            Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                            settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                          }
                          else
                          {
                            settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                          }

                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          Object value = settingValueAdapter.toValue(string);
                          if (value != null) list.add(value);
                        }
                        else if (listType == int.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          list.add(value);
                        }
                        else if (listType == Integer.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          int value = Integer.parseInt(string);
                          list.add(value);
                        }
                        else if (listType == long.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          long value = Long.parseLong(string);
                          list.add(value);
                        }
                        else if (listType == Long.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          long value = Long.parseLong(string);
                          list.add(value);
                        }
                        else if (listType == double.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          double value = Double.parseDouble(string);
                          list.add(value);
                        }
                        else if (listType == Long.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          double value = Double.parseDouble(string);
                          list.add(value);
                        }
                        else if (listType == boolean.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          boolean value = StringUtils.parseBoolean(string);
                          list.add(value);
                        }
                        else if (listType == Boolean.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          list.add(string);
                        }
                        else if (listType == String.class)
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          list.add(StringUtils.unescape(string));
                        }
                        else if (type == EnumSet.class)
                        {
                          // get value
                          Set set = (Set)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            set.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          Iterator iterator = StringUtils.parseEnumSet(listType,string).iterator();
                          while (iterator.hasNext())
                          {
                            set.add(iterator.next());
                          }
                        }
                        else if (listType.isEnum())
                        {
                          // get value
                          List list = (List)field.get(null);

                          // clear default value
                          if (hasDefaultFields.contains(field))
                          {
                            list.clear();
                            hasDefaultFields.remove(field);
                          }

                          // set value
                          list.add(StringUtils.parseEnum(type,string));
                        }
                        else
                        {
                          throw new Error(String.format("List '%s' without value adapter %s",field.getName(),listType));
                        }
                      }
                      else
                      {
                        // primitiv type
                        if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                        {
                          // instantiate config adapter class
                          ValueAdapter settingValueAdapter;
                          Class enclosingClass = settingValue.type().getEnclosingClass();
                          if (   (enclosingClass == Settings.class)
                              && settingValue.type().isMemberClass()
                              && !Modifier.isStatic(settingValue.type().getModifiers())
                             )
                          {
                            Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                            settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                          }
                          else
                          {
                            settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                          }

                          // convert to value
                          Object value = settingValueAdapter.toValue(string);
                          if (value != null) field.set(null,value);
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
                        else if (type == double.class)
                        {
                          double value = Double.parseDouble(string);
                          field.setDouble(null,value);
                        }
                        else if (type == Double.class)
                        {
                          double value = Double.parseDouble(string);
                          field.set(null,new Double(value));
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
                        else if (type == EnumSet.class)
                        {
                          Class enumClass = settingValue.type();
                          if (!enumClass.isEnum())
                          {
                            throw new Error(enumClass+" is not an enum class!");
                          }
                          field.set(null,StringUtils.parseEnumSet(enumClass,string));
                        }
                        else if (type.isEnum())
                        {
                          field.set(null,StringUtils.parseEnum(type,string));
                        }
                        else
                        {
//TODO: error message?
//Dprintf.dprintf("field.getType()=%s",type);
                        }
                      }
                    }
                    catch (NumberFormatException exception)
                    {
                      System.err.println(String.format("Cannot parse number '%s' for configuration value '%s' in line %d",string,name,lineNb));
                    }
                    catch (Throwable throwable)
                    {
                      System.err.println("INTERNAL ERROR at '"+file.getAbsolutePath()+"', line "+lineNb+": "+throwable.getMessage());
                      throwable.printStackTrace();
                      System.exit(ExitCodes.INTERNAL_ERROR);
                    }
                  }
                }
                else
                {
                  // ignore other annotations
                }
              }
            }
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
                  field.set(null,settingMigrate.run(field.get(null)));
                }
              }
              catch (Throwable throwable)
              {
                System.err.println("INTERNAL ERROR: migrate value '"+settingValue.name()+"': "+throwable.getMessage());
                throwable.printStackTrace();
                System.exit(ExitCodes.INTERNAL_ERROR);
              }
            }
          }
        }
      }
    }

    // reset modified, save last external modified time
    modifiedFlag = false;
    lastExternalModified = file.lastModified();
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

      // save settings
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

              if (!settingValue.deprecated())
              {
                // get value and write to file
                String name = (!settingValue.name().isEmpty()) ? settingValue.name() : field.getName();
                try
                {
                  Class type = field.getType();
//Dprintf.dprintf("field=%s type=%s",field,type);
                  if      (type.isArray())
                  {
                    // array type
                    type = type.getComponentType();
                    if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                    {
                      // instantiate config adapter class
                      ValueAdapter settingValueAdapter;
                      Class enclosingClass = settingValue.type().getEnclosingClass();
                      if (   (enclosingClass == Settings.class)
                          && settingValue.type().isMemberClass()
                          && !Modifier.isStatic(settingValue.type().getModifiers())
                         )
                      {
                        Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                        settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                      }
                      else
                      {
                        settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                      }

                      // convert to string
                      for (Object object : (Object[])field.get(null))
                      {
                        String value = (object != null) ? (String)settingValueAdapter.toString(object) : "";
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
                    else if (type == double.class)
                    {
                      for (double value : (double[])field.get(null))
                      {
                        output.printf("%s = %f\n",name,value);
                      }
                    }
                    else if (type == Double.class)
                    {
                      for (double value : (Double[])field.get(null))
                      {
                        output.printf("%s = %f\n",name,value);
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
                    else if (type == EnumSet.class)
                    {
                      for (EnumSet enumSet : (EnumSet[])field.get(null))
                      {
                        output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                      }
                    }
                    else if (type.isEnum())
                    {
                      for (Enum value : (Enum[])field.get(null))
                      {
                        output.printf("%s = %s\n",name,value.toString());
                      }
                    }
                    else
                    {
                      throw new Error(String.format("Array '%s' without type",field.getName()));
                    }
                  }
                  else if (type == HashSet.class)
                  {
                    Class<?> setType = (Class)(((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0]);
                    if     (setType != null)
                    {
                      if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                      {
                        // instantiate config adapter class
                        ValueAdapter settingValueAdapter;
                        Class enclosingClass = settingValue.type().getEnclosingClass();
                        if (   (enclosingClass == Settings.class)
                            && settingValue.type().isMemberClass()
                            && !Modifier.isStatic(settingValue.type().getModifiers())
                           )
                        {
                          Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                          settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                        }
                        else
                        {
                          settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                        }

                        // convert to string
                        HashSet<Object> hashSet = (HashSet<Object>)field.get(null);
                        for (Object object : hashSet)
                        {
                          String value = (String)settingValueAdapter.toString(object);
                          output.printf("%s = %s\n",name,value);
                        }
                      }
                      else if (setType == Integer.class)
                      {
                        HashSet<Integer> hashSet = (HashSet<Integer>)field.get(null);
                        for (int value : hashSet)
                        {
                          output.printf("%s = %d\n",name,value);
                        }
                      }
                      else if (setType == Long.class)
                      {
                        HashSet<Long> hashSet = (HashSet<Long>)field.get(null);
                        for (long value : hashSet)
                        {
                          output.printf("%s = %ld\n",name,value);
                        }
                      }
                      else if (setType == Boolean.class)
                      {
                        HashSet<Boolean> hashSet = (HashSet<Boolean>)field.get(null);
                        for (boolean value : hashSet)
                        {
                          output.printf("%s = %s\n",name,value ? "yes" : "no");
                        }
                      }
                      else if (setType == String.class)
                      {
                        HashSet<String> hashSet = (HashSet<String>)field.get(null);
                        for (String value : hashSet)
                        {
                          output.printf("%s = %s\n",name,StringUtils.escape(value));
                        }
                      }
                      else if (setType.isEnum())
                      {
                        HashSet<Enum> hashSet = (HashSet<Enum>)field.get(null);
                        for (Enum value : hashSet)
                        {
                          output.printf("%s = %s\n",name,value.toString());
                        }
                      }
                      else if (setType == EnumSet.class)
                      {
                        HashSet<EnumSet> hashSet = (HashSet<EnumSet>)field.get(null);
                        for (EnumSet enumSet : hashSet)
                        {
                          output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                        }
                      }
                      else
                      {
                        throw new Error(String.format("Hash set '%s' without value adapter %s",field.getName(),setType));
                      }
                    }
                    else
                    {
                      throw new Error(String.format("Hash set '%s' without type",field.getName()));
                    }
                  }
                  else if (type == ValueMap.class)
                  {
                    ValueMap<String> valueMap = (ValueMap<String>)field.get(null);
                    if (!valueMap.get().isEmpty())
                    {
                      output.printf("%s = %s\n",name,valueMap.get());
                    }
                    for (String key : valueMap.keySet())
                    {
                      output.printf("%s[%s] = %s\n",name,key,valueMap.get(key));
                    }
                  }
                  else if (Set.class.isAssignableFrom(type))
                  {
                    // Set type
                    Class<?> setType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                    if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                    {
                      // instantiate config adapter class
                      ValueAdapter settingValueAdapter;
                      Class enclosingClass = settingValue.type().getEnclosingClass();
                      if (   (enclosingClass == Settings.class)
                          && settingValue.type().isMemberClass()
                          && !Modifier.isStatic(settingValue.type().getModifiers())
                         )
                      {
                        Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                        settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                      }
                      else
                      {
                        settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                      }

                      // convert to string
                      for (Object object : (Set)field.get(null))
                      {
                        String value = (object != null) ? (String)settingValueAdapter.toString(object) : "";
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
                    else if (setType == double.class)
                    {
                      for (Object object : (Set)field.get(null))
                      {
                        output.printf("%s = %f\n",name,(Double)object);
                      }
                    }
                    else if (setType == Long.class)
                    {
                      for (Object object : (Set)field.get(null))
                      {
                        output.printf("%s = %double\n",name,(Double)object);
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
                    else if (type == EnumSet.class)
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((Set)field.get(null),","));
                    }
                    else if (setType.isEnum())
                    {
                      for (Object object : (Set)field.get(null))
                      {
                        output.printf("%s = %s\n",name,((Enum)object).toString());
                      }
                    }
                    else
                    {
                      throw new Error(String.format("Set '%s' without value adapter %s",field.getName(),setType));
                    }
                  }
                  else if (List.class.isAssignableFrom(type))
                  {
                    // List type
                    Class<?> listType = (Class<?>)((ParameterizedType)field.getGenericType()).getActualTypeArguments()[0];

                    if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                    {
                      // instantiate config adapter class
                      ValueAdapter settingValueAdapter;
                      Class enclosingClass = settingValue.type().getEnclosingClass();
                      if (   (enclosingClass == Settings.class)
                          && settingValue.type().isMemberClass()
                          && !Modifier.isStatic(settingValue.type().getModifiers())
                         )
                      {
                        Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                        settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                      }
                      else
                      {
                        settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                      }

                      // convert to string
                      for (Object object : (List)field.get(null))
                      {
                        String value = (object != null) ? (String)settingValueAdapter.toString(object) : "";
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
                    else if (listType == double.class)
                    {
                      for (Object object : (List)field.get(null))
                      {
                        output.printf("%s = %fn",name,(Double)object);
                      }
                    }
                    else if (listType == Double.class)
                    {
                      for (Object object : (List)field.get(null))
                      {
                        output.printf("%s = %f\n",name,(Double)object);
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
                    else if (type == EnumSet.class)
                    {
                      for (EnumSet enumSet : (List<EnumSet>)field.get(null))
                      {
                        output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                      }
                    }
                    else if (listType.isEnum())
                    {
                      for (Object object : (List)field.get(null))
                      {
                        output.printf("%s = %s\n",name,((Enum)object).toString());
                      }
                    }
                    else
                    {
                      throw new Error(String.format("List '%s' without value adapter %s",field.getName(),listType));
                    }
                  }
                  else
                  {
                    // primitiv type
                    if      (ValueAdapter.class.isAssignableFrom(settingValue.type()))
                    {
                      // instantiate config adapter class
                      ValueAdapter settingValueAdapter;
                      Class enclosingClass = settingValue.type().getEnclosingClass();
                      if (   (enclosingClass == Settings.class)
                          && settingValue.type().isMemberClass()
                          && !Modifier.isStatic(settingValue.type().getModifiers())
                         )
                      {
                        Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                        settingValueAdapter = (ValueAdapter)constructor.newInstance(new Settings());
                      }
                      else
                      {
                        settingValueAdapter = (ValueAdapter)settingValue.type().newInstance();
                      }

                      // convert to string
                      Object object = field.get(null);
                      String value = (object != null) ? (String)settingValueAdapter.toString(object) : "";
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
                    else if (type == double.class)
                    {
                      double value = field.getDouble(null);
                      output.printf("%s = %f\n",name,value);
                    }
                    else if (type == Double.class)
                    {
                      double value = (Double)field.get(null);
                      output.printf("%s = %f\n",name,value);
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
                    else if (type == EnumSet.class)
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((EnumSet)field.get(null),","));
                    }
                    else if (type.isEnum())
                    {
                      Enum value = (Enum)field.get(null);
                      output.printf("%s = %s\n",name,value.toString());
                    }
                    else
                    {
//Dprintf.dprintf("field.getType()=%s",type);
                    }
                  }
                }
                catch (Throwable throwable)
                {
Dprintf.dprintf("throwable=%s",throwable);
throwable.printStackTrace();
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
            else
            {
              // ignore other annotations
            }
          }
        }
      }

      // close file
      output.close(); output = null;

      // reset modfied, save last external modified time
      modifiedFlag = false;
      lastExternalModified = file.lastModified();
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

  /** permitt program settings will be modified
   */
  public static void permitModified()
  {
    // process all SWT events (like layouting etc.)
    Display display = Display.getCurrent();
    while (display.readAndDispatch())
    {
    }

    SettingUtils.permitModifyFlag = true;
  }

  /** check if program settings modify is permitted
   * @return true iff permitted
   */
  public static boolean isPermitModify()
  {
    return permitModifyFlag;
  }

  /** set program settings are modified
   */
  public static void setModified()
  {
    SettingUtils.modifiedFlag = true;
  }

  /** check if program settings are modified
   * @return true iff modified
   */
  public static boolean isModified()
  {
    return modifiedFlag;
  }

  /** check if program settings file is modified
   * @param file file
   * @return true iff modified
   */
  public static boolean isExternalModified(File file)
  {
    return (lastExternalModified != 0L) && (file.lastModified() > lastExternalModified);
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

  /** check if some name equals
   * @param names name array
   * @param string string
   * @return true if string equals some name
   */
  private static boolean nameEquals(String names[], String string)
  {
    for (String name : names)
    {
      if (name.equals(string)) return true;
    }
    return false;
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

  /** unique add element to double array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static double[] addArrayUniq(double[] array, double n)
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

  /** unique add element to double array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Double[] addArrayUniq(Double[] array, double n)
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

  /** unique add element to boolean array
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

  /** unique add element to boolean array
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

  /** add element to string array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static String[] addArray(String[] array, String string)
  {
    array = Arrays.copyOf(array,array.length+1);
    array[array.length-1] = string;

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
  private static Object[] addArrayUniq(Object[] array, Object object, ValueAdapter settingValueAdapter)
  {
    int i = 0;
    while ((i < array.length) && !settingValueAdapter.equals(array[i],object))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = object;
    }

    return array;
  }
}

/* end of file */
