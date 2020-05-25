const char htmlPage[] PROGMEM = "<!DOCTYPE html>"
"<html>"
    "<head>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=2\" />"
        "<style type=\"text/css\">"
            "button {"
                "background-color:greenyellow;"
                "width: 60px;"
            "}"
            ".adcVal {"
                "background-color:pink;"
                "text-align: center;"
            "}"
            ".errorclass {"
                "background-color:red;"
                "border-style: double;"
            "}"
        "</style>"
        "<script>"
            "var timeOutFunction;"
            "function display(id, text) {"
                "if (text === \"\") {"
                    "document.getElementById(id).style.display = \"none\";"
                "} else {"
                    "document.getElementById(id).style.display = \"inline\";"
                    "document.getElementById(id).innerHTML = \"&nbsp;&nbsp;&nbsp;&nbsp;\" + text + \"&nbsp;&nbsp;&nbsp;&nbsp;\";"
                "}"
            "}"
            "            "
            "function loadXMLDoc(query) {"
                "clearTimeout(timeOutFunction);"
                "var xhttp = new XMLHttpRequest();"
                "xhttp.onreadystatechange = function () {"
                    "if (this.readyState === 4 && this.status === 200) {"
                        "display(\"errorMessage\", \"\");"
                        "display(\"debugMessage\", \"\");"
                        "var obj = JSON.parse(this.responseText);"
                        "if (obj.on === undefined) {"
                            "var erm = \"\";"
                            "if (obj.err === undefined) {"
                                "erm = \"ERROR:Response not identified:[\" + this.responseText + \"]\";"
                            "} else {"
                                "erm = \"ERROR:[\" + this.responseText + \"]\";"
                            "}"
                            "console.error(erm);"
                            "display(\"errorMessage\", erm);"
                        "} else {"
                            "if (obj.debug) {"
                                "display(\"debugMessage\", \"DEBUG IS ON\");"
                            "}"
                            "document.getElementById(\"tabSA\").innerHTML = obj.on.sa;"
                            "document.getElementById(\"tabSB\").innerHTML = obj.on.sb;"
                            "document.getElementById(\"sense0\").innerHTML = obj.on.d0;"
                            "document.getElementById(\"sense1\").innerHTML = obj.on.d1;"
                            "document.getElementById(\"tabA0\").innerHTML = obj.on.t0;"
                            "document.getElementById(\"tabA1\").innerHTML = obj.on.t1;"
                            "document.getElementById(\"tabA2\").innerHTML = obj.on.v0;"
                            "document.getElementById(\"tabA3\").innerHTML = obj.on.v1;"
                        "}"
                    "}"
                "};"
                "xhttp.open(\"GET\", query, true);"
                "xhttp.send();"
                "timeOutFunction = setTimeout(function () {"
                    "loadXMLDoc('switch');"
                "}, 10000);"
            "}"
        "</script>"
        "<title>Device-$0</title>"
    "</head>"
    "<body onload=\"loadXMLDoc('switch')\">"
        "<h2>Device-$0</h2>"
        "<table>"
            "<tr>"
                "<td>$1</td>"
                "<td id=\"tabSA\">?</td>"
                "<td><button type=\"button\" onclick=\"loadXMLDoc('switch/sa=inv')\">Switch</button></td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td>$2</td>"
                "<td id=\"tabSB\">?</td>"
                "<td><button type=\"button\" onclick=\"loadXMLDoc('switch/sb=inv')\">Switch</button></td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td>ALL</td>"
                "<td></td>"
                "<td><button type=\"button\" onclick=\"loadXMLDoc('switch/all=on')\">ON</button></td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td>ALL</td>"
                "<td></td>"
                "<td><button type=\"button\" onclick=\"loadXMLDoc('switch/all=off')\">OFF</button></td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Sense 0</td>"
                "<td class=\"adcVal\" id=\"sense0\">?</td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Sense 1</td>"
                "<td class=\"adcVal\" id=\"sense1\">?</td>"
                "<td></td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Temperature 0</td>"
                "<td class=\"adcVal\" id=\"tabA0\">?</td>"
                "<td>(c)</td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Temperature 1</td>"
                "<td class=\"adcVal\" id=\"tabA1\">?</td>"
                "<td>(c)</td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Volts In 2</td>"
                "<td class=\"adcVal\" id=\"tabA2\">?</td>"
                "<td>&nbsp;v</td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Volts In 3</td>"
                "<td class=\"adcVal\" id=\"tabA3\">?</td>"
                "<td>&nbsp;v</td>"
            "</tr>"
            "<tr>"
                "<td colspan=\"2\">Reload State</td>"
                "<td><button type=\"button\" onclick=\"loadXMLDoc('switch')\">Refresh</button></td>"
                "<td></td>"
            "</tr>"
        "</table>"
        "<hr style=\"width:100%;text-align:left;margin-left:0\">"
        "<span class=\"errorclass\" id=\"errorMessage\"></span>"
        "<span class=\"errorclass\" id=\"debugMessage\"></span>"
    "</body>"
"</html>";