@REM Need to run this to copy the Java files over to our project
@REM where we can build and work with them.
@REM Later, we may choose to just copy around a JAR file containing the Java files

set SOARIO=..\..

xcopy %SOARIO%\ClientSMLSWIG\java\build\*.java sml\*.java /y

