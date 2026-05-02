#  Finds the specified number of the largest file at the selected path
#
# Created by Tukanos
#
# Input:
  # Ask the user for the number of files
  # Ask the user for path which will be searched
# Output:
  # Result is then printed in tmp file which is displayed
#
# date 12.02.2016 | version 0.0.1  init version
# date 12.02.2016 | version 0.0.2  better program flow and fixed errors
# date 15.02.2016 | version 0.0.3  fixed user cancel => Salamander.AbortScript() does not work (freezes salamander)
# date 15.02.2016 | version 0.0.4  Salamander compatible error handling
# date 15.02.2016 | version 0.0.5  Better cancelation handling -> even with the incorrect answer cancel button is active


class LargestFiles

  def initialize
    @fso = WIN32OLE.new('Scripting.FileSystemObject') # WIN32OLE FileSystemObject - init
    @drive_list = []
    @largest_files ={}
  end

  # run the application (workflow here)
  def run
    if get_files_count == :no # user did not press cancel button
      if get_path == :no # user did not press cancel button
        find_largest_file
        display_result
      end
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error running the script.\nError output: #{e.message}})
  end

private

  # Get user input - number of files (only integer values are valid)
  def get_files_count
    @input_files_count = Salamander.InputBox('Please Enter how many files you want in the result list', 'How many files to search for', '10')
    if @input_files_count == ''
      return :yes # user cancelled (return :yes)
    else
      user_answer = retest_files_count unless @input_files_count =~ /^\d+$/ # valid are only decimal chars
      cancel_button_pressed?(user_answer) # return value
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error getting user specified number of files.\nError output: #{e.message}})
  end

  # Select path where to search (will search also subdirectories)
  def get_path
    get_available_drives
    @selected_path = Salamander.InputBox("Please select your path.  The drives present: (#{@drive_list.to_s})", 'Select path', Salamander.SourcePanel.path)
    if @selected_path == ''
      return :yes # user cancelled (return :yes)
    else
      user_answer = restest_path unless Dir.exists?(@selected_path) # in case path does not exist ask user again
      cancel_button_pressed?(user_answer) # return value
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error getting user specified path.\nError output: #{e.message}})
  end

  # finding the largest files in the path specified by the user
  def find_largest_file
    Dir.chdir(@selected_path) # change directory to one specified by the user
    list_all_files = Dir.glob('**/*') # f# select all files at specified directory
    list_all_files = path_in_correct_format(list_all_files)
    file_size = list_all_files.map{|file_name| File.size(file_name)} # map all sizes
    store_largest_file(list_all_files,file_size)
  rescue Exception => e
    Salamander.MsgBox(%Q{Error finding the largest file.\nError output: #{e.message}})
  end
  
  # temp file management and showing the result to the user
  def display_result
    tmp_file_path, tmp_file_opened  = open_temp_file
    write_temp_file(tmp_file_opened)
    close_temp_file(tmp_file_opened,tmp_file_path)
  rescue Exception => e
    Salamander.MsgBox(%Q{Error displaying the result.\nError output: #{e.message}})
  end
  
  # defining tmp files
  def open_temp_file
    tmp_dir = @fso.GetSpecialFolder(2).Path
    tmp_file = @fso.GetTempName()
    log_file_path =  tmp_dir + tmp_file #open temp file
    return log_file_path, @fso.CreateTextFile(log_file_path, true) # log file path, log_file; true = overwrite
  rescue Exception => e
    Salamander.MsgBox(%Q{Error opening a temp file #{tmp_dir} or #{tmp_file}\nError output: #{e.message}})
  end

  # writing the result to temp file
  def write_temp_file(log_file)
    index = 1
    log_file.WriteLine("The largest files in directory #{@selected_path } (the largest first.)") # write to temp file an information message to the user
    @largest_files.each do |key, value| # write to temp file the actual result
      log_file.WriteLine("The #{index}. #{@selected_path}\\#{key} has a size #{value}.")
      index += 1
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error writing to the temp file: #{log_file}.\nError output: #{e.message}})
  end

  # temp file closing, viewing (showing the result to the user) and deleting
  def close_temp_file(log_file,log_file_path)
    log_file.Close(); # closing file
    Salamander.ViewFile(log_file_path, 1) # Displaying the actual result here
    @fso.DeleteFile(log_file_path) # deleting the file
  rescue Exception => e
    Salamander.MsgBox(%Q{Error closing the temp file: #{log_file}.\nError output: #{e.message}})
  end

  # convert linux path style / to windows path format \
  def path_in_correct_format(list_all_files)
    list_all_files.map{|path| path.gsub('/','\\')}
  rescue Exception => e
    Salamander.MsgBox(%Q{Error converting linux style path to windows style path.\nError output: #{e.message}})
  end

  # iterate specified number of times
  # collect the largest files in the @largest_files hash
  def store_largest_file(list_all_files,file_size)
    counter = (list_all_files.size < @input_files_count.to_i ? list_all_files.size : @input_files_count.to_i)
    counter.to_i.times do
      max_file_size = file_size.max # largest file
      file_index = file_size.index(max_file_size) # index of largest file
      @largest_files[list_all_files[file_index]] = readable_file_size(max_file_size) # hash: filename => 'file_size'
      list_all_files.delete_at(file_index) # delete largest file from list_all_files Array
      file_size.delete_at(file_index) #delete from file_size Array the largest file size
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error storing largest file.  From the all files: #{list_all_files} and the file sizes: #{file_size}\nError output: #{e.message}})
  end

  # converting a file_size in bytes to appropriate format
  def readable_file_size(file_size_bytes)
    count = 0
    while  file_size_bytes >= 1024 and count < 4
      file_size_bytes /= 1024.0
      count += 1
    end
    format("%.2f",file_size_bytes) + %w(B KB MB GB TB)[count]  # return properly formated
  rescue Exception => e
    Salamander.MsgBox(%Q{Error converting file to human readable format.\nError output: #{e.message}})
  end

  #  Get all drives available on the system
  def get_available_drives
    @fso.Drives.each { |drv| @drive_list << drv.DriveLetter } # select all drivers available
  rescue Exception => e
    Salamander.MsgBox(%Q{Error getting available drives on the system.\nError output: #{e.message}})
  end

  # Ask user again if the input is not an integer
  def retest_files_count
    loop do
      Salamander.MsgBox("Please enter Integer value", 0, 'Value entered is not an Interger.')  
      @input_files_count = Salamander.InputBox('Please Enter how many files you want in the result list', 'How many files to search for')
      break @input_files_count if @input_files_count =~ /^\d+$/ or @input_files_count = ''
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error asking user getting files count again.\nError output: #{e.message}})
  end

  # Ask user again if the path does not exist
  def restest_path
    loop do
      Salamander.MsgBox('The entered path does not exist.  Please enter a one that does.', 0, 'Value entered is not an Interger.')  
      @selected_path = Salamander.InputBox("Please existing path)", 'Select path', Salamander.SourcePanel.path)
      break @selected_path if Dir.exists?(@selected_path) or @selected_path = ''
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error asking user getting for path again.\nError output: #{e.message}})
  end

  # did user pressed cancel button?
  def cancel_button_pressed?(user_answer)
    if user_answer == ''
      return :yes # user cancelled
    else
      return :no # user did not cancel
    end
  rescue Exception => e
    Salamander.MsgBox(%Q{Error finding if cancel button pressed.\nError output: #{e.message}})
  end
end

app = LargestFiles.new
app.run