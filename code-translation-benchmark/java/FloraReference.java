import java.awt.Image;
import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDate;
import javax.imageio.ImageIO;
import javax.swing.JFrame;
import javax.swing.SwingUtilities;
import javax.swing.Timer;

public final class FloraReference {
    private static Path assetDirectory() throws IOException {
        try {
            Path application = Path.of(FloraReference.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            return application.getParent().resolve("assets/ims");
        } catch (URISyntaxException e) {
            throw new IOException("Cannot locate application assets", e);
        }
    }

    private static Image image(String name) throws IOException {
        Path path = assetDirectory().resolve(name + ".gif");
        if (!Files.isRegularFile(path)) throw new IOException("Missing asset: " + path);
        Image image = ImageIO.read(path.toFile());
        if (image == null) throw new IOException("Invalid asset: " + path);
        return image;
    }

    private static cal2panel panel() throws IOException {
        Image[] r = new Image[10], b = new Image[10];
        for (int i = 0; i < 10; i++) { r[i] = image("r" + i); b[i] = image("b" + i); }
        cal2panel p = new cal2panel(image("shiseido"), image("maeda2"), r, b);
        p.setSize(400, 400);
        return p;
    }

    private static void fixtures(Path scenario, Path output) throws IOException {
        Files.createDirectories(output);
        BufferedImage atlas = new BufferedImage(400, 400, BufferedImage.TYPE_INT_RGB);
        Graphics graphics = atlas.getGraphics();
        for (int i = 0; i < 10; i++) {
            graphics.drawImage(image("b" + i), 10 + 20*i, 20, null);
            graphics.drawImage(image("r" + i), 10 + 20*i, 60, null);
        }
        graphics.drawImage(image("maeda2"), 10, 100, null);
        graphics.drawImage(image("shiseido"), 50, 100, null);
        graphics.dispose();
        ImageIO.write(atlas, "png", output.resolve("decoded-assets.png").toFile());
        cal2panel p = panel();
        for (String line : Files.readAllLines(scenario)) {
            if (line.isBlank() || line.startsWith("#")) continue;
            String[] fields = line.split("\\s+");
            FloraClock.time = Long.parseLong(fields[0]);
            p.mx = Integer.parseInt(fields[1]); p.my = Integer.parseInt(fields[2]);
            for (int i = 0; i < Integer.parseInt(fields[3]); i++) p.mouseUp(null, p.mx, p.my);
            FloraClock.fixedDate = LocalDate.of(2024, Integer.parseInt(fields[4]), Integer.parseInt(fields[5]));
            String name = fields[6];
            if (name.equals("-")) { p.renderFrame(); continue; }
            try (PrintWriter trace = new PrintWriter(Files.newBufferedWriter(output.resolve(name + ".geometry")))) {
                FloraClock.geometry = trace;
                ImageIO.write(p.renderFrame(), "png", output.resolve(name + ".png").toFile());
            } finally { FloraClock.geometry = null; }
        }
    }

    private static void run(boolean smoke) throws IOException {
        cal2panel p = panel();
        JFrame window = new JFrame("John Maeda — Flora Calendar (Java reference)");
        window.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        window.setContentPane(p);
        window.setResizable(false);
        window.pack();
        window.setLocationByPlatform(true);
        final int[] frames = {0};
        Timer timer = new Timer(5, e -> {
            FloraClock.tick();
            p.renderFrame();
            p.repaint();
            if (smoke && ++frames[0] == 100) window.dispose();
        });
        window.addWindowListener(new WindowAdapter() {
            @Override public void windowClosed(WindowEvent e) { timer.stop(); }
        });
        window.setVisible(true);
        timer.start();
    }

    public static void main(String[] args) throws Exception {
        SwingUtilities.invokeAndWait(() -> {
            try {
                if (args.length == 3 && args[0].equals("--fixtures")) {
                    fixtures(Path.of(args[1]), Path.of(args[2]));
                } else if (args.length == 0 || (args.length == 1 && args[0].equals("--smoke-test"))) {
                    run(args.length == 1);
                } else throw new IllegalArgumentException("Usage: [--smoke-test | --fixtures SCENARIO OUTPUT]");
            } catch (IOException e) { throw new RuntimeException(e); }
        });
    }
}
