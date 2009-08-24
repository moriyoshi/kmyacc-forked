/*
 * Integer calculator
 * You can compile "Calc.as" by "mxmlc Calc.as" with Flex2 SDK.
 */

%{
package
{
	import flash.display.*;
	import flash.text.*;
	public class Calc extends MovieClip
	{
		public function Calc()
		{
			instance = this;
			var cp:CalcParser = new CalcParser();
			cp.inputString = "" +
				"1 + 2 * 3\n" +
				"4 * 5 + 6\n" +
				"3 * 10 * 2\n";
			CalcParser.yydebug = true;
			CalcParser.yyDumpParseTree = true;
			cp.yyparse();
		}
		// output function
		private static var instance:Calc;
		private static var _output_y:Number = 10;
		public static function output(msg:String):void
		{
			var t:TextField = new TextField();
			t.autoSize = TextFieldAutoSize.LEFT;
			t.x = 10;
			t.y = _output_y;
			t.text = msg;
			_output_y += t.height;
			instance.addChild(t);
		}
	}
}
%}

%token NUMBER
%left '+' '-'
%left '*' '/'

%%
start	:	lines;
lines	:	/* empty */
		|	lines line { yylrec++; }
		;

line	:	expr '\n' { Calc.output("result = " + $1); }
		|	'\n' { trace("(empty line ignored)"); }
		|	error '\n'
		;

expr	:	expr '+' expr { $$ = $1 + $3; }
		|	expr '-' expr { $$ = $1 - $3; }
		|	expr '*' expr { $$ = $1 * $3; }
		|	expr '/' expr { $$ = $1 / $3; }
		|	'(' expr ')' { $$ = $2; }
		|	NUMBER { $$ = $1; }
		;

%%

/* Lexical analyzer */
public var inputString:String;
private var inputPos:int=0;
private var ch:String = null;

private function getch():String {
  var c:String = ch;
  if (c != null) {
    ch = null;
    return c;
  }
  if(inputPos >= inputString.length) {
    return null;
  }
  return inputString.charAt(inputPos++);
}

private function ungetch(c:String):void { ch = c; }

private function isDigit(s:String):Boolean {
  var c:String = s.charAt(0);
  return ("0" <= c && c <= "9");
}

private function yylex():int {
  yylval = null;
  for (;;) {
    var c:String = getch();
    if (c == null) {
      return 0;
    } else if (c == ' ' || c == '\t') {
      // skip
    } else if (isDigit(c)) {
      var n:String = "";
      while (c != null && isDigit(c)) {
        n += c;
        c = getch();
      }
      ungetch(c);
      yylval = parseInt(n);
      return NUMBER;
    } else {
      return c.charCodeAt(0);
    }
  }
  throw "Doesn't come here";
}

private function yyerror(msg:String):void {
  trace(msg);
}
