// Import ESM modules
import express from 'express';
import { join } from 'node:path';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import cors from 'cors';

// Get useful file paths
const this_file_name = fileURLToPath(import.meta.url);
const root = dirname(this_file_name);
const html_file = join(root, 'index.html')


// Create an Express application, as opposed to using the lower-level http module
const app = express();
// This allows using the HTML file that sets up the canvas for WebGL
app.use(express.static(root));

// Allow CORS for all routes (for development purposes)
app.use(cors());
app.get('/vertex.glsl', (req, res) => {
  res.json({ message: 'This is accessible from any origin' });
});
app.get('/fragment.glsl', (req, res) => {
  res.json({ message: 'This is accessible from any origin' });
});

// Serve the HTML file
app.get('/', (req, res) => {
  res.sendFile(html_file)
});


// Start the server
const port = parseInt(process.env.PORT) || 8080;
app.listen(port, () => {
  console.log(`helloworld: listening on port ${port}`);
});
