import Py4GW


def update():
    pass


def draw():
    visible = Py4GW.ImGui.begin("Py4GW Python Smoke Test")
    if visible:
        Py4GW.ImGui.text("Python draw() is running inside the ImGui frame.")
        if Py4GW.ImGui.button("Send Message To Console"):
            Py4GW.Console.notice("PythonTest", "Button clicked from imgui_console_smoke_test.py")
            Py4GW.Console.print("Python print bridge reached the console.")
    Py4GW.ImGui.end()
