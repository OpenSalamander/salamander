# viz https://forum.altap.cz/viewtopic.php?f=6&t=31928

class TestScope
  attr_reader :wscript_obj
  def initialize(wscript_obj)
    @wscript_obj=wscript_obj
  end
  def do
    # Objekt WScript neni zde ve tride k dispozici...
    #WScript.Echo("In Class")
    # ALE máme na nìj odkaz wscript_obj
    wscript_obj.Echo("In Class") # již v pohodì funguje
  end
end

WScript.Echo("GLOBAL")

cls = TestScope.new(WScript)
cls.do
