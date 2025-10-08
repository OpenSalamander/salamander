# see https://forum.altap.cz/viewtopic.php?f=6&t=31928

class TestScope
  attr_reader :wscript_obj
  def initialize(wscript_obj)
    @wscript_obj=wscript_obj
  end
  def do
    # The WScript object is not available here in the class...
    #WScript.Echo("In Class")
    # But we have a reference to it through wscript_obj
    wscript_obj.Echo("In Class") # and this works just fine
  end
end

WScript.Echo("GLOBAL")

cls = TestScope.new(WScript)
cls.do
