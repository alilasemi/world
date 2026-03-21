// Import ESM modules
import express from 'express';
import { join } from 'node:path';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';


// Get useful file paths
const this_file_name = fileURLToPath(import.meta.url);
const root = dirname(this_file_name);
const html_file = join(root, 'index.html')


// Create an Express application, as opposed to using the lower-level http module
const app = express();
// This allows using the HTML file that sets up the canvas for WebGL
app.use(express.static(root));


// Serve the HTML file
app.get('/', (req, res) => {
  res.sendFile(html_file)
});


// Start the server
const port = parseInt(process.env.PORT) || 8080;
app.listen(port, () => {
  console.log(`helloworld: listening on port ${port}`);
});
