import http.server

EdgePlayer = http.server.SimpleHTTPRequestHandler

EdgePlayer.extensions_map['.js'] = 'text/javascript'
EdgePlayer.extensions_map['.mjs'] = 'text/javascript'

http.server.test(EdgePlayer, port=8000)