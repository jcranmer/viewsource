include("beautify.js");

function process_js(ast) {
  _print('// process_js');
  _print(js_beautify(uneval(ast))
    .replace(/(start|end): {[ \t\n]*line: ([0-9]*),[ \t\n]*column: ([0-9]*)[ \t\n]*}/g, '$1: location($2:$3)')
    .replace(/{/g, '<span>{')
    .replace(/}/g, '}</span>'));
}
