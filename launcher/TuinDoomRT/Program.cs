using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace TuinDoomRT
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            if (WelcomeForm.ShouldShow)
            {
                using (var welcome = new WelcomeForm())
                {
                    if (welcome.ShowDialog() != DialogResult.OK) return;
                }
            }
            Application.Run(new LauncherForm());
        }
    }

    internal sealed class IwadOption
    {
        public string Path { get; set; }
        public string Game { get; set; }
        public override string ToString() { return Game + "  —  " + Path; }
    }

    internal sealed class CinematicPictureBox : PictureBox
    {
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(BackColor);
            if (Image == null) return;
            float scale = Math.Max((float)ClientSize.Width / Image.Width, (float)ClientSize.Height / Image.Height);
            int width = (int)Math.Ceiling(Image.Width * scale);
            int height = (int)Math.Ceiling(Image.Height * scale);
            int x = (ClientSize.Width - width) / 2;
            int y = (ClientSize.Height - height) / 2;
            e.Graphics.DrawImage(Image, new Rectangle(x, y, width, height));
        }
    }

    internal sealed class SkyBackgroundPanel : Panel
    {
        private Image sky;

        public SkyBackgroundPanel()
        {
            DoubleBuffered = true;
            string assetDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets");
            string[] skies = Directory.Exists(assetDir)
                ? Directory.GetFiles(assetDir, "launcher-sky-*.png")
                : new string[0];
            if (skies.Length > 0)
            {
                try
                {
                    string selected = skies[new Random(Guid.NewGuid().GetHashCode()).Next(skies.Length)];
                    using (var source = Image.FromFile(selected)) sky = new Bitmap(source);
                }
                catch { sky = null; }
            }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            e.Graphics.Clear(Color.FromArgb(13, 17, 18));
            if (sky == null) return;
            float scale = Math.Max((float)ClientSize.Width / sky.Width, (float)ClientSize.Height / sky.Height);
            int width = (int)Math.Ceiling(sky.Width * scale);
            int height = (int)Math.Ceiling(sky.Height * scale);
            int x = (ClientSize.Width - width) / 2;
            int y = (ClientSize.Height - height) / 2;
            e.Graphics.DrawImage(sky, new Rectangle(x, y, width, height));
            using (var shade = new SolidBrush(Color.FromArgb(174, 5, 7, 8)))
                e.Graphics.FillRectangle(shade, ClientRectangle);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && sky != null) sky.Dispose();
            base.Dispose(disposing);
        }
    }

    internal sealed class AccentGroupBox : GroupBox
    {
        private static readonly Color Card = Color.FromArgb(205, 8, 12, 13);
        private static readonly Color Accent = Color.FromArgb(255, 105, 30);

        public AccentGroupBox()
        {
            SetStyle(ControlStyles.SupportsTransparentBackColor |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint, true);
            BackColor = Color.Transparent;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            var body = new Rectangle(0, 9, Width - 1, Height - 10);
            using (var fill = new SolidBrush(Card)) e.Graphics.FillRectangle(fill, body);
            using (var border = new Pen(Accent)) e.Graphics.DrawRectangle(border, body);

            SizeF titleSize = e.Graphics.MeasureString(Text, Font);
            var titleBack = new RectangleF(12, 0, titleSize.Width + 10, titleSize.Height + 1);
            using (var fill = new SolidBrush(Color.FromArgb(235, 8, 12, 13)))
                e.Graphics.FillRectangle(fill, titleBack);
            using (var brush = new SolidBrush(Accent))
                e.Graphics.DrawString(Text, Font, brush, 17, 0);
        }
    }

    internal sealed class WelcomeForm : Form
    {
        private static readonly string PreferenceFile = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "TuinDoomRT", "show-welcome.txt");
        private readonly CheckBox showAgain = new CheckBox();

        public static bool ShouldShow
        {
            get
            {
                try { return !File.Exists(PreferenceFile) || File.ReadAllText(PreferenceFile).Trim() != "0"; }
                catch { return true; }
            }
        }

        public WelcomeForm()
        {
            Text = "Welcome to TuinDOOM RT 1.4.7.8";
            ClientSize = new Size(960, 720);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(10, 13, 14);
            ForeColor = Color.White;
            Font = new Font("Segoe UI", 9F);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

            var title = new Label {
                Text = "TUINDOOM RT 1.4.7.8", ForeColor = Color.White,
                Font = new Font("Segoe UI", 23F, FontStyle.Bold), AutoSize = true, Location = new Point(28, 18)
            };
            var subtitle = new Label {
                Text = "FINAL STABLE BUILD  •  RAY TRACING FOR EVERY WAD", ForeColor = Color.FromArgb(255, 105, 30),
                Font = new Font("Consolas", 10F, FontStyle.Bold), AutoSize = true, Location = new Point(31, 60)
            };
            var art = new CinematicPictureBox {
                Location = new Point(28, 92), Size = new Size(904, 430), BackColor = Color.Black
            };
            string assets = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets");
            string[] pictures = Directory.Exists(assets)
                ? Directory.GetFiles(assets, "intro-*.png").OrderBy(x => x).ToArray()
                : new string[0];
            if (pictures.Length > 0)
            {
                try { art.Image = Image.FromFile(pictures[new Random().Next(pictures.Length)]); } catch { }
            }
            var limitations = new Label {
                Text = "STABLE BUILD NOTES\r\n" +
                       "• Requires a legal DOOM or DOOM II IWAD; no commercial game data is included.\r\n" +
                       "• General WADs use automatic RT lighting, so results naturally vary by map.\r\n" +
                       "• Experimental liquid shaders and newer raw voxel packs are disabled for stability.\r\n" +
                       "• N cycles the outdoor or authored light direction. F toggles the flashlight.",
                ForeColor = Color.FromArgb(220, 228, 229), Location = new Point(30, 540), Size = new Size(900, 94),
                Font = new Font("Segoe UI", 10F)
            };
            showAgain.Text = "Show this screen at startup";
            showAgain.Checked = true;
            showAgain.ForeColor = Color.FromArgb(160, 175, 178);
            showAgain.AutoSize = true;
            showAgain.Location = new Point(31, 663);
            var proceed = new Button {
                Text = "CONTINUE TO LAUNCHER", Location = new Point(676, 650), Size = new Size(256, 44),
                FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(145, 255, 22), ForeColor = Color.Black,
                DialogResult = DialogResult.OK, Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            proceed.FlatAppearance.BorderColor = Color.FromArgb(145, 255, 22);
            proceed.Click += delegate {
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(PreferenceFile));
                    File.WriteAllText(PreferenceFile, showAgain.Checked ? "1" : "0");
                }
                catch { }
            };
            AcceptButton = proceed;
            Controls.AddRange(new Control[] { title, subtitle, art, limitations, showAgain, proceed });
        }
    }

    internal sealed class IwadPickerForm : Form
    {
        private readonly ListBox list = new ListBox();
        public IwadOption SelectedIwad { get { return list.SelectedItem as IwadOption; } }

        public IwadPickerForm(IEnumerable<IwadOption> options)
        {
            Text = "Choose your Doom IWAD";
            ClientSize = new Size(760, 390);
            MinimumSize = new Size(620, 340);
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Color.FromArgb(13, 17, 18);
            ForeColor = Color.White;
            Font = new Font("Segoe UI", 9F);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            var title = new Label {
                Text = "SELECT A MAIN GAME", ForeColor = Color.FromArgb(255, 105, 30),
                Font = new Font("Segoe UI", 17F, FontStyle.Bold), AutoSize = true, Location = new Point(22, 18)
            };
            var hint = new Label {
                Text = "The scanner found these legal IWADs. Choose which Doom game to launch.",
                ForeColor = Color.FromArgb(150, 170, 174), AutoSize = true, Location = new Point(25, 57)
            };
            list.SetBounds(24, 88, 712, 230);
            list.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            list.BackColor = Color.FromArgb(7, 11, 12); list.ForeColor = Color.White; list.BorderStyle = BorderStyle.None;
            foreach (IwadOption option in options) list.Items.Add(option);
            if (list.Items.Count > 0) list.SelectedIndex = 0;
            var cancel = new Button { Text = "CANCEL", DialogResult = DialogResult.Cancel, Location = new Point(506, 336), Size = new Size(105, 34), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(22, 27, 28), ForeColor = Color.White, Anchor = AnchorStyles.Bottom | AnchorStyles.Right };
            var select = new Button { Text = "USE SELECTED", DialogResult = DialogResult.OK, Location = new Point(620, 336), Size = new Size(116, 34), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(145, 255, 22), ForeColor = Color.Black, Anchor = AnchorStyles.Bottom | AnchorStyles.Right };
            cancel.FlatAppearance.BorderColor = Color.FromArgb(255, 105, 30);
            select.FlatAppearance.BorderColor = Color.FromArgb(145, 255, 22);
            select.Enabled = list.Items.Count > 0;
            list.DoubleClick += delegate { if (list.SelectedItem != null) { DialogResult = DialogResult.OK; Close(); } };
            AcceptButton = select; CancelButton = cancel;
            Controls.AddRange(new Control[] { title, hint, list, cancel, select });
        }
    }

    internal sealed class Profile
    {
        public string Name { get; set; }
        public string EnginePath { get; set; }
        public string IwadPath { get; set; }
        public List<string> Mods { get; set; }
        public bool AutoSun { get; set; }
        public int AutoSunIntensity { get; set; }
        public int AutoSunSeed { get; set; }
        public string SunCycleKey { get; set; }
        public bool? CinematicRays { get; set; }
        public bool? StockDoom2Scenes { get; set; }
        public bool? ColoredSkyLighting { get; set; }
        public bool? RealisticE1Lights { get; set; }
        public bool? RealisticE2Lights { get; set; }
        public bool? RealisticE3Lights { get; set; }
        public bool? RealisticDoom2Lights { get; set; }
        public bool CeilingLights { get; set; }
        public int CeilingLightIntensity { get; set; }
        public string Upscaler { get; set; }
        public string BloodEffects { get; set; }
        public string ExtraArguments { get; set; }
        public Profile()
        {
            Mods = new List<string>(); AutoSun = true; AutoSunIntensity = 100; SunCycleKey = "N"; CinematicRays = true; ColoredSkyLighting = true;
            CeilingLights = true; CeilingLightIntensity = 75; Upscaler = "DLSS Quality";
            BloodEffects = "Fluids + wall decals"; ExtraArguments = "";
        }
        public override string ToString() { return Name; }
    }

    internal sealed class LauncherData
    {
        public List<Profile> Profiles { get; set; }
        public string LastProfile { get; set; }
        public LauncherData() { Profiles = new List<Profile>(); }
    }

    internal sealed class LauncherForm : Form
    {
        private static readonly Color Back = Color.FromArgb(13, 17, 18);
        private static readonly Color Panel = Color.FromArgb(22, 27, 28);
        private static readonly Color Field = Color.FromArgb(7, 11, 12);
        private static readonly Color Orange = Color.FromArgb(255, 105, 30);
        private static readonly Color Lime = Color.FromArgb(145, 255, 22);
        private static readonly Color TextColor = Color.FromArgb(225, 231, 232);
        private static readonly Color Muted = Color.FromArgb(128, 155, 160);

        private readonly string configDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "TuinDoomRT");
        private string ConfigFile { get { return Path.Combine(configDir, "profiles.json"); } }

        private LauncherData data = new LauncherData();
        private readonly ListBox profileList = new ListBox();
        private readonly TextBox profileName = new TextBox();
        private readonly TextBox enginePath = new TextBox();
        private readonly ComboBox iwadList = new ComboBox();
        private readonly ListBox modList = new ListBox();
        private readonly CheckBox autoSun = new CheckBox();
        private readonly TrackBar sunIntensity = new TrackBar();
        private readonly Label sunValue = new Label();
        private readonly NumericUpDown sunSeed = new NumericUpDown();
        private readonly ComboBox sunCycleKey = new ComboBox();
        private readonly CheckBox cinematicRays = new CheckBox();
        private readonly CheckBox stockDoom2Scenes = new CheckBox();
        private readonly CheckBox ceilingLights = new CheckBox();
        private readonly TrackBar ceilingIntensity = new TrackBar();
        private readonly Label ceilingValue = new Label();
        private readonly ComboBox upscaler = new ComboBox();
        private readonly ComboBox bloodEffects = new ComboBox();
        private readonly CheckBox coloredSkyLighting = new CheckBox();
        private readonly CheckBox realisticE1Lights = new CheckBox();
        private readonly CheckBox realisticE2Lights = new CheckBox();
        private readonly CheckBox realisticE3Lights = new CheckBox();
        private readonly CheckBox realisticDoom2Lights = new CheckBox();
        private readonly TextBox extraArgs = new TextBox();
        private readonly TextBox commandPreview = new TextBox();
        private readonly Label status = new Label();
        private bool loading;

        private const int LockedSunIntensity = 95;
        private const int LockedCeilingIntensity = 75;
        private static readonly string[] BundledModNames = {
            "nashgore.pk3",
            "nashgore_rt_voxel_compat.pk3"
        };

        public LauncherForm()
        {
            Text = "TuinDoom RT";
            ClientSize = new Size(1180, 820);
            MinimumSize = new Size(1120, 760);
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Back;
            ForeColor = TextColor;
            Font = new Font("Segoe UI", 9F);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            AllowDrop = true;
            DragEnter += OnDragEnter;
            DragDrop += OnDragDrop;

            BuildUi();
            LoadData();
            FindEngine();
            ScanIwads(true);
            UpdatePreview();
        }

        private void BuildUi()
        {
            var header = new Panel { Dock = DockStyle.Top, Height = 96, BackColor = Color.FromArgb(7, 10, 11) };
            var face = new PictureBox {
                Location = new Point(14, 5), Size = new Size(86, 86), SizeMode = PictureBoxSizeMode.Zoom,
                BackColor = Color.Transparent
            };
            string facePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets", "tuindoom-launcher-art.png");
            if (File.Exists(facePath)) face.Image = Image.FromFile(facePath);
            var title = new Label {
                Text = "TUINDOOM RT", ForeColor = Color.White, Font = new Font("Segoe UI", 24F, FontStyle.Bold),
                Location = new Point(116, 17), AutoSize = true
            };
            var subtitle = new Label {
                Text = "RAY TRACING FOR EVERY WAD", ForeColor = Orange,
                Font = new Font("Consolas", 10F, FontStyle.Bold), Location = new Point(120, 61), AutoSize = true
            };
            var drop = new Label {
                Text = "DROP .WAD / .PK3 FILES ANYWHERE", ForeColor = Muted,
                Font = new Font("Consolas", 9F), Anchor = AnchorStyles.Top | AnchorStyles.Right,
                Location = new Point(830, 29), AutoSize = true
            };
            header.Controls.AddRange(new Control[] { face, title, subtitle, drop });
            Controls.Add(header);

            var left = new Panel { Dock = DockStyle.Left, Width = 224, Padding = new Padding(12), BackColor = Panel };
            left.Controls.Add(MakeLabel("SAVED PROFILES", 12, 12));
            profileList.Location = new Point(12, 38);
            profileList.Size = new Size(200, 430);
            profileList.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            StyleList(profileList);
            profileList.SelectedIndexChanged += delegate { LoadSelectedProfile(); };
            left.Controls.Add(profileList);
            var newButton = MakeButton("+ NEW", 12, 482, 96, 38, Panel);
            var deleteButton = MakeButton("DELETE", 116, 482, 96, 38, Panel);
            newButton.Anchor = deleteButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Left;
            newButton.Click += delegate { NewProfile(); };
            deleteButton.Click += delegate { DeleteProfile(); };
            left.Controls.AddRange(new Control[] { newButton, deleteButton });
            Controls.Add(left);

            var main = new SkyBackgroundPanel { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(28, 18, 28, 22) };
            Controls.Add(main);
            main.BringToFront();

            int y = 18;
            AddFieldLabel(main, "PROFILE NAME", ref y);
            profileName.SetBounds(28, y, 820, 28); StyleText(profileName); y += 37;
            main.Controls.Add(profileName);

            AddFieldLabel(main, "MAIN GAME IWAD  (SELECT DOOM OR DOOM II)", ref y);
            iwadList.SetBounds(28, y, 600, 30); StyleCombo(iwadList);
            var scan = MakeButton("SCAN", 638, y, 100, 30, Panel);
            var browseIwad = MakeButton("BROWSE", 748, y, 100, 30, Panel);
            scan.Click += delegate { ScanIwads(false); };
            browseIwad.Click += delegate { BrowseIwad(); };
            iwadList.SelectedIndexChanged += delegate { UpdatePreview(); };
            main.Controls.AddRange(new Control[] { iwadList, scan, browseIwad }); y += 40;

            AddFieldLabel(main, "MOD LOAD ORDER  (TOP LOADS FIRST)", ref y);
            modList.SetBounds(28, y, 600, 96); StyleList(modList);
            modList.AllowDrop = true; modList.DragEnter += OnDragEnter; modList.DragDrop += OnDragDrop;
            var addMod = MakeButton("+ ADD", 638, y, 100, 34, Lime, Color.Black);
            var removeMod = MakeButton("REMOVE", 748, y, 100, 34, Panel);
            var upMod = MakeButton("MOVE UP", 638, y + 39, 100, 34, Panel);
            var downMod = MakeButton("MOVE DOWN", 748, y + 39, 100, 34, Panel);
            addMod.Click += delegate { AddModsWithDialog(); };
            removeMod.Click += delegate { RemoveSelectedMod(); };
            upMod.Click += delegate { MoveMod(-1); };
            downMod.Click += delegate { MoveMod(1); };
            var bundledMods = new Label {
                Text = "BUILT-IN VISUAL MODS INCLUDED",
                ForeColor = Lime, Font = new Font("Consolas", 8.5F, FontStyle.Bold),
                Location = new Point(28, y + 101), AutoSize = true
            };
            main.Controls.AddRange(new Control[] { modList, addMod, removeMod, upMod, downMod, bundledMods }); y += 132;

            // Production lighting is intentionally locked to the tested preset.
            autoSun.Checked = true;
            sunIntensity.Minimum = 75; sunIntensity.Maximum = 500; sunIntensity.Value = LockedSunIntensity;
            sunSeed.Minimum = -9999; sunSeed.Maximum = 9999; sunSeed.Value = 0;
            ceilingLights.Checked = true;
            ceilingIntensity.Minimum = 0; ceilingIntensity.Maximum = 300; ceilingIntensity.Value = LockedCeilingIntensity;
            sunCycleKey.Items.AddRange(new object[] { "N", "J", "K", "L", "F11", "None" });
            sunCycleKey.SelectedIndex = 0;
            cinematicRays.Checked = true;
            stockDoom2Scenes.Checked = false;

            var lighting = new AccentGroupBox {
                Text = " RAY-TRACED LIGHTING ", ForeColor = Orange,
                Location = new Point(28, y), Size = new Size(820, 143)
            };
            realisticE1Lights.Appearance = Appearance.Button;
            realisticE1Lights.Text = "REALISTIC E1 LIGHTS";
            realisticE1Lights.TextAlign = ContentAlignment.MiddleCenter;
            realisticE1Lights.SetBounds(18, 25, 230, 34);
            realisticE1Lights.FlatStyle = FlatStyle.Flat;
            realisticE1Lights.BackColor = Panel;
            realisticE1Lights.ForeColor = TextColor;
            realisticE1Lights.FlatAppearance.BorderColor = Orange;
            realisticE1Lights.CheckedChanged += delegate {
                realisticE1Lights.BackColor = realisticE1Lights.Checked ? Lime : Panel;
                realisticE1Lights.ForeColor = realisticE1Lights.Checked ? Color.Black : TextColor;
            };
            realisticE2Lights.Appearance = Appearance.Button;
            realisticE2Lights.Text = "REALISTIC E2 LIGHTS";
            realisticE2Lights.TextAlign = ContentAlignment.MiddleCenter;
            realisticE2Lights.SetBounds(264, 25, 230, 34);
            realisticE2Lights.FlatStyle = FlatStyle.Flat;
            realisticE2Lights.BackColor = Panel;
            realisticE2Lights.ForeColor = TextColor;
            realisticE2Lights.FlatAppearance.BorderColor = Orange;
            realisticE2Lights.CheckedChanged += delegate {
                realisticE2Lights.BackColor = realisticE2Lights.Checked ? Lime : Panel;
                realisticE2Lights.ForeColor = realisticE2Lights.Checked ? Color.Black : TextColor;
            };
            realisticE3Lights.Appearance = Appearance.Button;
            realisticE3Lights.Text = "REALISTIC E3 LIGHTS";
            realisticE3Lights.TextAlign = ContentAlignment.MiddleCenter;
            realisticE3Lights.SetBounds(510, 25, 230, 34);
            realisticE3Lights.FlatStyle = FlatStyle.Flat;
            realisticE3Lights.BackColor = Panel;
            realisticE3Lights.ForeColor = TextColor;
            realisticE3Lights.FlatAppearance.BorderColor = Orange;
            realisticE3Lights.CheckedChanged += delegate {
                realisticE3Lights.BackColor = realisticE3Lights.Checked ? Lime : Panel;
                realisticE3Lights.ForeColor = realisticE3Lights.Checked ? Color.Black : TextColor;
            };
            realisticDoom2Lights.Appearance = Appearance.Button;
            realisticDoom2Lights.Text = "REALISTIC DOOM II SKIES + LIGHTS  (MAP01–32)";
            realisticDoom2Lights.TextAlign = ContentAlignment.MiddleCenter;
            realisticDoom2Lights.SetBounds(18, 67, 722, 34);
            realisticDoom2Lights.FlatStyle = FlatStyle.Flat;
            realisticDoom2Lights.BackColor = Panel;
            realisticDoom2Lights.ForeColor = TextColor;
            realisticDoom2Lights.FlatAppearance.BorderColor = Orange;
            realisticDoom2Lights.CheckedChanged += delegate {
                realisticDoom2Lights.BackColor = realisticDoom2Lights.Checked ? Lime : Panel;
                realisticDoom2Lights.ForeColor = realisticDoom2Lights.Checked ? Color.Black : TextColor;
            };
            var presetInfo = new Label {
                Text = "REALISTIC PRESETS APPLY TO ORIGINAL MAPS     •     N CYCLES THE LIGHT DIRECTION",
                ForeColor = Lime, Font = new Font("Consolas", 10F, FontStyle.Bold),
                Location = new Point(18, 112), AutoSize = true
            };
            lighting.Controls.AddRange(new Control[] { realisticE1Lights, realisticE2Lights, realisticE3Lights, realisticDoom2Lights, presetInfo });
            main.Controls.Add(lighting); y += 157;

            var blood = new AccentGroupBox {
                Text = " BLOOD EFFECTS ", ForeColor = Orange,
                Location = new Point(28, y), Size = new Size(820, 66)
            };
            blood.Controls.Add(MakeLabel("Mode", 18, 31));
            bloodEffects.SetBounds(70, 26, 330, 28); StyleCombo(bloodEffects);
            bloodEffects.Items.AddRange(new object[] {
                "Fluids + wall decals",
                "Fluids only",
                "Wall decals only (no fluids)",
                "Disable fluids and wall decals"
            });
            bloodEffects.SelectedIndex = 0;
            var bloodHint = new Label {
                Text = "Changes ray-traced blood fluids and impact decals.",
                ForeColor = Muted, BackColor = Color.Transparent,
                Location = new Point(420, 31), AutoSize = true
            };
            blood.Controls.AddRange(new Control[] { bloodEffects, bloodHint });
            main.Controls.Add(blood); y += 78;

            var render = new AccentGroupBox {
                Text = " UPSCALING ", ForeColor = Orange,
                Location = new Point(28, y), Size = new Size(820, 86)
            };
            render.Controls.Add(MakeLabel("Mode", 18, 31));
            upscaler.SetBounds(70, 26, 250, 28); StyleCombo(upscaler);
            upscaler.Items.AddRange(new object[] { "DLSS Quality", "DLSS Balanced", "DLSS Performance", "FSR Quality", "FSR Balanced", "FSR Performance" });
            upscaler.SelectedIndex = 0;
            coloredSkyLighting.Text = "Colored sky lighting (original/custom skies)";
            coloredSkyLighting.SetBounds(350, 28, 350, 24);
            coloredSkyLighting.ForeColor = TextColor;
            coloredSkyLighting.BackColor = Color.Transparent;
            coloredSkyLighting.Checked = true;
            var frameGenWarning = new Label {
                Text = "FRAME GENERATION MAY DISTORT HUD AND WEAPON SPRITES — DISABLE IT IN THE RT MENU IF NEEDED.",
                ForeColor = Orange, BackColor = Color.Transparent,
                Font = new Font("Consolas", 8.5F, FontStyle.Bold),
                Location = new Point(18, 65), AutoSize = true
            };
            render.Controls.AddRange(new Control[] { upscaler, coloredSkyLighting, frameGenWarning }); main.Controls.Add(render); y += 98;

            var save = MakeButton("SAVE PROFILE", 28, y, 260, 44, Panel);
            var play = MakeButton("PLAY NOW", 304, y, 544, 44, Lime, Color.Black);
            save.Click += delegate { SaveCurrentProfile(); };
            play.Click += delegate { LaunchGame(); };
            main.Controls.AddRange(new Control[] { save, play });
            status.SetBounds(28, y + 51, 820, 26); status.ForeColor = Muted; status.BackColor = Color.Transparent; status.Text = "READY"; main.Controls.Add(status);

            foreach (Control c in new Control[] { profileName, enginePath, autoSun, sunIntensity, sunSeed, sunCycleKey, ceilingLights, ceilingIntensity, upscaler, bloodEffects, coloredSkyLighting, realisticE1Lights, realisticE2Lights, realisticE3Lights, realisticDoom2Lights, cinematicRays, stockDoom2Scenes, extraArgs })
            {
                if (c is TextBox) ((TextBox)c).TextChanged += delegate { UpdatePreview(); };
                else if (c is CheckBox) ((CheckBox)c).CheckedChanged += delegate { UpdatePreview(); };
                else if (c is TrackBar) ((TrackBar)c).ValueChanged += delegate { UpdatePreview(); };
                else if (c is NumericUpDown) ((NumericUpDown)c).ValueChanged += delegate { UpdatePreview(); };
                else if (c is ComboBox) ((ComboBox)c).SelectedIndexChanged += delegate { UpdatePreview(); };
            }
        }

        private Label MakeLabel(string text, int x, int y)
        {
            return new Label { Text = text, ForeColor = Muted, BackColor = Color.FromArgb(8, 12, 13), Padding = new Padding(3, 1, 3, 1), Location = new Point(x, y), AutoSize = true, Font = new Font("Segoe UI", 8F, FontStyle.Bold) };
        }

        private void AddFieldLabel(Control parent, string text, ref int y)
        {
            parent.Controls.Add(MakeLabel(text, 28, y)); y += 19;
        }

        private Button MakeButton(string text, int x, int y, int w, int h, Color color, Color? foreground = null)
        {
            var b = new Button { Text = text, Location = new Point(x, y), Size = new Size(w, h), FlatStyle = FlatStyle.Flat, BackColor = color, ForeColor = foreground ?? TextColor, Cursor = Cursors.Hand };
            b.FlatAppearance.BorderColor = color == Lime ? Lime : Orange;
            b.FlatAppearance.BorderSize = 1;
            b.FlatAppearance.MouseOverBackColor = color == Lime ? Color.FromArgb(177, 255, 82) : Color.FromArgb(37, 44, 45);
            b.FlatAppearance.MouseDownBackColor = color == Lime ? Color.FromArgb(110, 210, 15) : Color.FromArgb(255, 105, 30);
            return b;
        }

        private void StyleText(TextBox box) { box.BackColor = Field; box.ForeColor = TextColor; box.BorderStyle = BorderStyle.FixedSingle; }
        private void StyleCombo(ComboBox box) { box.BackColor = Field; box.ForeColor = TextColor; box.DropDownStyle = ComboBoxStyle.DropDownList; box.FlatStyle = FlatStyle.Flat; }
        private void StyleList(ListBox box)
        {
            box.BackColor = Field; box.ForeColor = TextColor; box.BorderStyle = BorderStyle.None; box.IntegralHeight = false;
            box.DrawMode = DrawMode.OwnerDrawFixed;
            box.ItemHeight = 22;
            box.DrawItem += delegate(object sender, DrawItemEventArgs e) {
                if (e.Index < 0) return;
                bool selected = (e.State & DrawItemState.Selected) != 0;
                using (var fill = new SolidBrush(selected ? Lime : Field)) e.Graphics.FillRectangle(fill, e.Bounds);
                using (var brush = new SolidBrush(selected ? Color.Black : TextColor))
                    e.Graphics.DrawString(box.Items[e.Index].ToString(), box.Font, brush, e.Bounds.X + 7, e.Bounds.Y + 3);
            };
        }

        private void LoadData()
        {
            try
            {
                if (File.Exists(ConfigFile)) data = new JavaScriptSerializer().Deserialize<LauncherData>(File.ReadAllText(ConfigFile)) ?? new LauncherData();
            }
            catch { data = new LauncherData(); }
            RefreshProfiles();
            if (data.Profiles.Count > 0)
            {
                int index = data.Profiles.FindIndex(p => p.Name == data.LastProfile);
                profileList.SelectedIndex = index >= 0 ? index : 0;
            }
            else NewProfile();
        }

        private void PersistData()
        {
            Directory.CreateDirectory(configDir);
            File.WriteAllText(ConfigFile, new JavaScriptSerializer().Serialize(data));
        }

        private void RefreshProfiles()
        {
            profileList.Items.Clear();
            foreach (Profile p in data.Profiles.OrderBy(p => p.Name)) profileList.Items.Add(p);
        }

        private void NewProfile()
        {
            loading = true;
            profileList.ClearSelected(); profileName.Text = "New profile"; modList.Items.Clear();
            autoSun.Checked = true; sunIntensity.Value = LockedSunIntensity; sunSeed.Value = 0;
            sunCycleKey.SelectedIndex = 0;
            ceilingLights.Checked = true; ceilingIntensity.Value = 75; upscaler.SelectedIndex = 0; cinematicRays.Checked = true; stockDoom2Scenes.Checked = false;
            bloodEffects.SelectedIndex = 0;
            coloredSkyLighting.Checked = true;
            realisticE1Lights.Checked = false; realisticE2Lights.Checked = false; realisticE3Lights.Checked = false; realisticDoom2Lights.Checked = false; extraArgs.Text = "";
            loading = false; FindEngine(); UpdatePreview();
        }

        private void LoadSelectedProfile()
        {
            var p = profileList.SelectedItem as Profile; if (p == null) return;
            loading = true;
            profileName.Text = p.Name; modList.Items.Clear();
            foreach (string mod in p.Mods ?? new List<string>()) modList.Items.Add(mod);
            autoSun.Checked = true; sunIntensity.Value = LockedSunIntensity; sunSeed.Value = 0;
            sunCycleKey.SelectedItem = "N";
            ceilingLights.Checked = true; ceilingIntensity.Value = LockedCeilingIntensity;
            int up = upscaler.Items.IndexOf(p.Upscaler ?? "DLSS Quality"); upscaler.SelectedIndex = up >= 0 ? up : 0;
            int blood = bloodEffects.Items.IndexOf(p.BloodEffects ?? "Fluids + wall decals"); bloodEffects.SelectedIndex = blood >= 0 ? blood : 0;
            coloredSkyLighting.Checked = p.ColoredSkyLighting ?? true;
            realisticE1Lights.Checked = p.RealisticE1Lights ?? false; realisticE2Lights.Checked = p.RealisticE2Lights ?? false; realisticE3Lights.Checked = p.RealisticE3Lights ?? false; realisticDoom2Lights.Checked = p.RealisticDoom2Lights ?? false; cinematicRays.Checked = true; stockDoom2Scenes.Checked = false; extraArgs.Text = "";
            FindEngine(); SelectIwadPath(p.IwadPath); loading = false; UpdatePreview();
        }

        private Profile ReadProfile()
        {
            var selected = iwadList.SelectedItem as IwadOption;
            return new Profile {
                Name = string.IsNullOrWhiteSpace(profileName.Text) ? "Profile" : profileName.Text.Trim(),
                EnginePath = ResolveEnginePath(), IwadPath = selected == null ? "" : selected.Path,
                Mods = modList.Items.Cast<string>().ToList(), AutoSun = true,
                AutoSunIntensity = LockedSunIntensity, AutoSunSeed = 0,
                SunCycleKey = "N",
                CeilingLights = true, CeilingLightIntensity = LockedCeilingIntensity,
                Upscaler = upscaler.SelectedItem == null ? "DLSS Quality" : upscaler.SelectedItem.ToString(),
                BloodEffects = bloodEffects.SelectedItem == null ? "Fluids + wall decals" : bloodEffects.SelectedItem.ToString(),
                ColoredSkyLighting = coloredSkyLighting.Checked,
                RealisticE1Lights = realisticE1Lights.Checked, RealisticE2Lights = realisticE2Lights.Checked, RealisticE3Lights = realisticE3Lights.Checked, RealisticDoom2Lights = realisticDoom2Lights.Checked, CinematicRays = true, StockDoom2Scenes = false, ExtraArguments = ""
            };
        }

        private void SaveCurrentProfile()
        {
            Profile p = ReadProfile();
            int old = data.Profiles.FindIndex(x => string.Equals(x.Name, p.Name, StringComparison.OrdinalIgnoreCase));
            if (old >= 0) data.Profiles[old] = p; else data.Profiles.Add(p);
            data.LastProfile = p.Name; PersistData(); RefreshProfiles();
            for (int i = 0; i < profileList.Items.Count; i++) if (((Profile)profileList.Items[i]).Name == p.Name) profileList.SelectedIndex = i;
            SetStatus("PROFILE SAVED", Lime);
        }

        private void DeleteProfile()
        {
            var p = profileList.SelectedItem as Profile; if (p == null) return;
            data.Profiles.RemoveAll(x => x.Name == p.Name); PersistData(); RefreshProfiles(); NewProfile(); SetStatus("PROFILE DELETED", Orange);
        }

        private void FindEngine()
        {
            enginePath.Text = ResolveEnginePath();
        }

        private string ResolveEnginePath()
        {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string[] candidates = {
                Path.Combine(baseDir, "Game", "gzdoom.exe"), Path.Combine(baseDir, "Game", "gzdoom-autosun.exe"),
                Path.GetFullPath(Path.Combine(baseDir, "..", "..", "build", "test-release", "gzdoom.exe"))
            };
            return candidates.FirstOrDefault(File.Exists) ?? Path.Combine(baseDir, "Game", "gzdoom.exe");
        }

        private IEnumerable<string> SteamRoots()
        {
            var roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string[] defaults = { @"C:\Program Files (x86)\Steam", @"C:\Program Files\Steam" };
            foreach (string root in defaults) if (Directory.Exists(root)) roots.Add(root);
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
                {
                    string value = key == null ? null : key.GetValue("SteamPath") as string;
                    if (!string.IsNullOrEmpty(value)) roots.Add(value.Replace('/', '\\'));
                }
            }
            catch { }
            foreach (string root in roots.ToArray())
            {
                string vdf = Path.Combine(root, "steamapps", "libraryfolders.vdf");
                if (!File.Exists(vdf)) continue;
                try
                {
                    foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\"")) roots.Add(m.Groups[1].Value.Replace("\\\\", "\\"));
                }
                catch { }
            }
            return roots;
        }

        private void ScanIwads(bool quiet)
        {
            var selectedBeforeScan = iwadList.SelectedItem as IwadOption;
            string previous = selectedBeforeScan == null ? null : selectedBeforeScan.Path;
            var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string root in SteamRoots())
            {
                string common = Path.Combine(root, "steamapps", "common");
                string[] candidates = {
                    Path.Combine(common, "Ultimate Doom", "rerelease", "doom.wad"),
                    Path.Combine(common, "Ultimate Doom", "rerelease", "DOOM2.WAD"),
                    Path.Combine(common, "Ultimate Doom", "base", "DOOM.WAD"),
                    Path.Combine(common, "Ultimate Doom", "base", "doom2", "DOOM2.WAD"),
                    Path.Combine(common, "DOOM 2", "base", "DOOM2.WAD"),
                    Path.Combine(common, "DOOM 2", "rerelease", "DOOM2.WAD")
                };
                foreach (string c in candidates) if (File.Exists(c)) paths.Add(c);
            }
            var options = paths.Select(p => new IwadOption { Path = p, Game = DetectGame(p) })
                .Where(x => x.Game != null)
                .OrderBy(x => x.Game)
                .ThenBy(x => x.Path.IndexOf("\\rerelease\\", StringComparison.OrdinalIgnoreCase) >= 0 ? 0 : 1)
                .ThenBy(x => x.Path)
                .ToList();
            iwadList.Items.Clear(); foreach (IwadOption option in options) iwadList.Items.Add(option);
            SelectIwadPath(previous);
            if (iwadList.SelectedIndex < 0 && iwadList.Items.Count > 0) iwadList.SelectedIndex = 0;
            if (!quiet)
            {
                SetStatus(options.Count > 0 ? options.Count + " IWAD(S) FOUND" : "NO IWADS FOUND — USE BROWSE", options.Count > 0 ? Lime : Orange);
                if (options.Count > 0)
                {
                    using (var picker = new IwadPickerForm(options))
                    {
                        if (picker.ShowDialog(this) == DialogResult.OK && picker.SelectedIwad != null)
                        {
                            SelectIwadPath(picker.SelectedIwad.Path);
                            SetStatus("SELECTED " + picker.SelectedIwad.Game.ToUpperInvariant(), Lime);
                        }
                    }
                }
            }
        }

        private string DetectGame(string path)
        {
            try
            {
                using (var fs = File.OpenRead(path)) using (var br = new BinaryReader(fs, Encoding.ASCII))
                {
                    string sig = new string(br.ReadChars(4)); if (sig != "IWAD" && sig != "PWAD") return null;
                    int count = br.ReadInt32(); int directory = br.ReadInt32(); bool e1m1 = false, map01 = false;
                    fs.Position = directory;
                    for (int i = 0; i < count; i++)
                    {
                        br.ReadInt32(); br.ReadInt32(); string name = Encoding.ASCII.GetString(br.ReadBytes(8)).TrimEnd('\0').ToUpperInvariant();
                        if (name == "E1M1") e1m1 = true; if (name == "MAP01") map01 = true;
                    }
                    if (e1m1) return "DOOM (1993)"; if (map01) return "DOOM II";
                }
            }
            catch { }
            return null;
        }

        private string PreferRereleaseIwad(string path)
        {
            if (string.IsNullOrEmpty(path) || path.IndexOf("\\rerelease\\", StringComparison.OrdinalIgnoreCase) >= 0) return path;
            int basePart = path.IndexOf("\\base\\", StringComparison.OrdinalIgnoreCase);
            if (basePart < 0) return path;
            string gameRoot = path.Substring(0, basePart);
            string candidate = Path.Combine(gameRoot, "rerelease", Path.GetFileName(path));
            return File.Exists(candidate) ? candidate : path;
        }

        private void SelectIwadPath(string path)
        {
            if (string.IsNullOrEmpty(path)) return;
            for (int i = 0; i < iwadList.Items.Count; i++) if (string.Equals(((IwadOption)iwadList.Items[i]).Path, path, StringComparison.OrdinalIgnoreCase)) { iwadList.SelectedIndex = i; return; }
            if (File.Exists(path)) { var option = new IwadOption { Path = path, Game = DetectGame(path) ?? "IWAD" }; iwadList.Items.Add(option); iwadList.SelectedItem = option; }
        }

        private void BrowseEngine()
        {
            using (var d = new OpenFileDialog { Filter = "GZDoom executable|gzdoom*.exe|Executable|*.exe" }) if (d.ShowDialog() == DialogResult.OK) enginePath.Text = d.FileName;
        }

        private void BrowseIwad()
        {
            using (var d = new OpenFileDialog { Filter = "Doom IWAD|*.wad" }) if (d.ShowDialog() == DialogResult.OK) SelectIwadPath(d.FileName);
        }

        private void AddModsWithDialog()
        {
            using (var d = new OpenFileDialog { Filter = "Doom mods|*.wad;*.pk3;*.pk7|All files|*.*", Multiselect = true }) if (d.ShowDialog() == DialogResult.OK) AddMods(d.FileNames);
        }

        private void AddMods(IEnumerable<string> files)
        {
            foreach (string file in files.Where(f => File.Exists(f) && new[] { ".wad", ".pk3", ".pk7" }.Contains(Path.GetExtension(f).ToLowerInvariant()))) if (!modList.Items.Contains(file)) modList.Items.Add(file);
            UpdatePreview();
        }

        private void RemoveSelectedMod() { if (modList.SelectedIndex >= 0) modList.Items.RemoveAt(modList.SelectedIndex); UpdatePreview(); }
        private void MoveMod(int direction)
        {
            int from = modList.SelectedIndex, to = from + direction; if (from < 0 || to < 0 || to >= modList.Items.Count) return;
            object item = modList.Items[from]; modList.Items.RemoveAt(from); modList.Items.Insert(to, item); modList.SelectedIndex = to; UpdatePreview();
        }

        private void OnDragEnter(object sender, DragEventArgs e) { if (e.Data.GetDataPresent(DataFormats.FileDrop)) e.Effect = DragDropEffects.Copy; }
        private void OnDragDrop(object sender, DragEventArgs e) { AddMods((string[])e.Data.GetData(DataFormats.FileDrop)); }

        private string Q(string value) { return "\"" + (value ?? "").Replace("\"", "\\\"") + "\""; }
        private IEnumerable<string> BundledModPaths()
        {
            string engine = ResolveEnginePath();
            string gameDir = Path.GetDirectoryName(engine) ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Game");
            return BundledModNames.Select(name => Path.Combine(gameDir, "Mods", name));
        }

        private List<string> MissingBundledMods()
        {
            return BundledModPaths().Where(path => !File.Exists(path)).ToList();
        }

        private string BuildArguments()
        {
            var iwad = iwadList.SelectedItem as IwadOption; if (iwad == null) return "";
            string engine = ResolveEnginePath();
            string gameDir = Path.GetDirectoryName(engine) ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Game");
            string privateConfig = Path.Combine(gameDir, "tuindoom-rt.ini");
            var a = new List<string> { "-config", Q(privateConfig), iwad.Game.StartsWith("DOOM II") ? "-rtdoom2" : "-rtdoom1", "-iwad", Q(iwad.Path) };
            // User-selected map/gameplay mods load before the core RT package.
            // Raw KVX/VOX packages are deliberately
            // excluded: the RT renderer requires converted GLTF models and otherwise
            // produces misplaced/corrupt models and a severe-performance warning.
            var files = new List<string>();
            files.AddRange(modList.Items.Cast<string>());
            files.AddRange(BundledModPaths().Where(File.Exists));
            foreach (string file in files)
            {
                // This RT fork consumes one path per -file switch.
                a.Add("-file");
                a.Add(Q(file));
            }
            a.AddRange(new[] { "+rt_classic", "0", "+rt_stockscenes", "0", "+rt_sky_saturation", coloredSkyLighting.Checked ? "1" : "0", "+rt_doom_e1_realistic_lights", realisticE1Lights.Checked ? "1" : "0", "+rt_doom_e2_realistic_lights", realisticE2Lights.Checked ? "1" : "0", "+rt_doom_e3_realistic_lights", realisticE3Lights.Checked ? "1" : "0", "+rt_doom2_realistic_lights", realisticDoom2Lights.Checked ? "1" : "0", "+rt_sun", "1", "+rt_sun_a", "15", "+rt_sun_b", "0", "+rt_sun_intensity", LockedSunIntensity.ToString(), "+rt_sun_color", Q("ff a0 60"), "+rt_autosun", "0", "+rt_autosun_seed", "0", "+rt_emis_mapboost", "200", "+rt_emis_maxscrcolor", "8", "+rt_ceilinglights", "1", "+rt_ceilinglight_intensity", LockedCeilingIntensity.ToString(), "+rt_vsync", "0", "+rt_hdr", "0", "+tuindoom_flashlight_dust", "1", "+rt_flsh", "0", "+rt_flsh_intensity", "200", "+rt_flsh_angle", "35", "+rt_volume_type", "1", "+rt_volume_scatter", "1", "+rt_volume_ambient", "0.03", "+rt_volume_lintensity", "1", "+rt_volume_lassymetry", "0.5", "+rt_bloom", "1", "+rt_bloom_scale", "1", "+exec", Q("tuindoom-bindings.cfg") });
            string scale = upscaler.SelectedItem == null ? "DLSS Quality" : upscaler.SelectedItem.ToString();
            int dlss = scale.StartsWith("DLSS") ? (scale.EndsWith("Quality") ? 1 : scale.EndsWith("Balanced") ? 2 : 3) : 0;
            int fsr = scale.StartsWith("FSR") ? (scale.EndsWith("Quality") ? 1 : scale.EndsWith("Balanced") ? 2 : 3) : 0;
            a.AddRange(new[] { "+rt_upscale_dlss", dlss.ToString(), "+rt_upscale_fsr2", fsr.ToString() });
            string blood = bloodEffects.SelectedItem == null ? "Fluids + wall decals" : bloodEffects.SelectedItem.ToString();
            bool fluids = blood == "Fluids + wall decals" || blood == "Fluids only";
            int bloodReplacement = blood == "Fluids + wall decals" ? 1 : blood == "Wall decals only (no fluids)" ? 2 : 0;
            a.AddRange(new[] {
                "+rt_fluid", fluids ? "1" : "0",
                "+rt_blood_repl", bloodReplacement.ToString(),
                "+cl_bloodsplats", bloodReplacement > 0 ? "1" : "0",
                "+m_use_mouse", "2"
            });
            return string.Join(" ", a);
        }

        private void UpdatePreview()
        {
            if (loading) return;
            sunValue.Text = sunIntensity.Value.ToString(); ceilingValue.Text = ceilingIntensity.Value.ToString();
            commandPreview.Text = Q(enginePath.Text) + " " + BuildArguments();
        }

        private void LaunchGame()
        {
            string exe = ResolveEnginePath();
            enginePath.Text = exe;
            var iwad = iwadList.SelectedItem as IwadOption;
            if (!File.Exists(exe)) { MessageBox.Show("Select the installed ray-traced GZDoom executable.", "Engine not found", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            if (iwad == null || !File.Exists(iwad.Path)) { MessageBox.Show("Select a valid Doom or Doom II IWAD.", "IWAD not found", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            List<string> missing = MissingBundledMods();
            if (missing.Count > 0) { MessageBox.Show("The bundled visual mods are missing:\n\n" + string.Join("\n", missing) + "\n\nRepair or reinstall TuinDoom RT.", "Bundled mods not found", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            try
            {
                SaveCurrentProfile();
                Process.Start(new ProcessStartInfo { FileName = exe, Arguments = BuildArguments(), WorkingDirectory = Path.GetDirectoryName(exe), UseShellExecute = true });
                SetStatus("GAME STARTED", Lime);
            }
            catch (Exception ex) { MessageBox.Show(ex.Message, "Could not start Doom", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }

        private void SetStatus(string text, Color color) { status.Text = text; status.ForeColor = color; }
    }
}
