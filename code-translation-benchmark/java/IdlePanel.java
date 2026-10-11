import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.image.BufferedImage;
import javax.swing.JPanel;

/** EDT-owned replacement for the applet's unsafe drawing thread. */
abstract class IdlePanel extends JPanel {
    public int mx, my;
    private BufferedImage frame;

    IdlePanel() {
        setPreferredSize(new Dimension(400, 400));
        MouseAdapter input = new MouseAdapter() {
            private void position(MouseEvent e) { mx = e.getX(); my = e.getY(); }
            @Override public void mouseMoved(MouseEvent e) { position(e); }
            @Override public void mouseDragged(MouseEvent e) { position(e); }
            @Override public void mousePressed(MouseEvent e) { position(e); }
            @Override public void mouseReleased(MouseEvent e) {
                position(e);
                IdlePanel.this.mouseUp(null, mx, my);
            }
        };
        addMouseListener(input);
        addMouseMotionListener(input);
    }

    public abstract void redraw(Graphics g);

    BufferedImage renderFrame() {
        if (frame == null) frame = new BufferedImage(400, 400, BufferedImage.TYPE_INT_RGB);
        Graphics g = frame.getGraphics();
        try { redraw(g); } finally { g.dispose(); }
        return frame;
    }

    @Override protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        if (frame != null) g.drawImage(frame, 0, 0, null);
    }
}
