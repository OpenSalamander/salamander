// Tests handling of invalid input passed to a GUI component constructor
// (white-box test for an exception thrown from the component C++ constructor)
var fm = Salamander.Forms.Form();
fm.cancelbtn = Salamander.Forms.Button("Cancel", "a");
