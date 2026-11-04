using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
internal static class RadioTheme {
    internal static readonly Color Ink=Color.FromArgb(13,23,35),Panel=Color.FromArgb(30,52,72),Text=Color.FromArgb(237,240,243),Gold=Color.FromArgb(220,201,142),Cyan=Color.FromArgb(66,224,237);
    internal static GraphicsPath Round(RectangleF r,float radius){var p=new GraphicsPath();float d=radius*2;p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.Right-d,r.Y,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.X,r.Bottom-d,d,d,90,90);p.CloseFigure();return p;}
    internal static void Label(Graphics g,string text,Font font,Rectangle box,Color color,bool center=false){
        TextRenderer.DrawText(g,text,font,box,color,TextFormatFlags.NoPrefix|TextFormatFlags.EndEllipsis|TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter|(center?TextFormatFlags.HorizontalCenter:TextFormatFlags.Left));
    }
}
internal sealed class RadioButton:Button {
    internal bool Accent;
    internal RadioButton(){SetStyle(ControlStyles.SupportsTransparentBackColor,true);FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;Font=new Font("Microsoft YaHei UI",10,FontStyle.Bold);ForeColor=RadioTheme.Text;BackColor=Color.Transparent;}
    protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
        using(var shape=RadioTheme.Round(new RectangleF(1,1,Width-3,Height-3),Math.Min(20,Height/2-1)))
        using(var fill=new SolidBrush(Accent?Color.FromArgb(237,239,240):Color.FromArgb(24,35,49)))
        using(var line=new Pen(Enabled?(Accent?Color.FromArgb(238,239,242):Color.FromArgb(138,151,166)):Color.FromArgb(61,72,84),1.4f)){
            g.FillPath(fill,shape);g.DrawPath(line,shape);
        }
        RadioTheme.Label(g,Text,Font,ClientRectangle,Enabled?(Accent?Color.FromArgb(17,22,28):ForeColor):Color.FromArgb(103,116,130),true);
    }
}
