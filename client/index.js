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

// Command line argument
const mode = process.argv[2] || 'gcloud';

// Get the server's IP. If running locally, use localhost. Otherwise, get the
// external IP of the gcloud instance.
var ip;
if (mode === 'local') {
    ip = 'localhost';
} else if (mode === 'gcloud') {
    const response = await fetch('https://api.ipify.org');
    ip = await response.text();
} else {
    console.error(`Unknown mode: ${mode}`);
    process.exit(1);
}

app.get('/config', (req, res) => {
    res.json({
        mode: mode,
        ip: ip,
    });
});
console.log(`Running in ${mode} mode, server IP is ${ip}`);

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
