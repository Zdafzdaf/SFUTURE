   M S B A M L   `   `   `  ���� A  =Client, Version=1.0.1.0, Culture=neutral, PublicKeyToken=nullclr-namespace:ClientClient      Client.PatchesWindow   R NWindowsBase, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35W SPresentationCore, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35D 9http://schemas.microsoft.com/winfx/2006/xaml/presentation    R NSystem.Xaml, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     @d2http://schemas.microsoft.com/expression/blend/2008     Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     &localclr-namespace:Client       � Title$#  Менеджер патчей��$	��500��$	��475��$	��500��$	��500��$	��475��$	��475������  j� Stretch$
 Fill=� �� ImageSource$( "Resources/LoginPage_Background.png��.���  � RowDefinitions �� $
��400* ��� $	��25* �.+��� -   $��PatchesList��$��Stretch=�$��0��$��Stretch=���Z�  � 	Resources  �� Color��%v   (��       *SystemColors.ControlBrushKey+)(��      * SystemColors.HighlightBrushKey+)��Red���� $	 Red�� �� View ��    GridViewColumnHeaderStyle1! �� ColumnHeaderContainerStyle# [  .P��� $��Patch�� �� Width$	 300��	 �� DisplayMemberBinding	 �� *	PatchName+�� $��Files��$ 80��	 �� *
PatchCount+�� $��Active��$ 80��	 �� *PatchActive+�� $��Удалить патч��$��1��$
��Left=�$	��Top=�$	��101��
 J� RenderTransformOrigin$
 0.508,-0.161&�$��27�����	#FF2F2F2F$��0q�}�. 
��������� $#��Установить патч��$��1��$
��Left=�$	��Top=�$	��101��$
 0.508,-0.161&�$��27�����	#FF2F2F2F$��0q�}�. 
�������$��	106,0,0,0q�