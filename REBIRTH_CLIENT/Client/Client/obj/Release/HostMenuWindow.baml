   M S B A M L   `   `   `  ���� A  =Client, Version=1.0.1.0, Culture=neutral, PublicKeyToken=nullclr-namespace:ClientClient      Client.HostMenuWindow   4mah,http://metro.mahapps.com/winfx/xaml/controls  R NWindowsBase, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35W SPresentationCore, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35D 9http://schemas.microsoft.com/winfx/2006/xaml/presentation    R NSystem.Xaml, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     @d2http://schemas.microsoft.com/expression/blend/2008     Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     &localclr-namespace:Client     -     � Title$  HostMenuWindow��$��321.26��$��426.378��$	��480��$	��640�� � 	Resources  �� 
TargetType �� Property �� Value �� RadiusY �� RadiusX �� 
TargetType �� CornerRadius	 J� SnapsToDevicePixels
 ��Name � RowDefinitions ��Name �� ColumnCollection �� Columns �� Content �� Triggers P� Property P� Value �� EndPoint �� 
StartPoint � Color � Offset �� 
TargetName ]� 
Conditions �� Property �� Value��%   '��       �� " ��.���  �@�FocusVisualStyle �� .���  � �� �� .R��� $ 2��$ 2�����	#8E6EA6F5$��1����  � ��
 ���� ��  � ��
 ���� ��  � ��$ 1q���  � ��$ 0,0,0,1q���  � ��$	 5,2q���  � ��$ Center=���  � �� �� " ��.R��� #��z��#��z��#��z��$ 2��	 . ._��� $
 InnerBorder��$��1q�$ 1��._�� $��24��$��24�� �� �� .+��� $ UpperHighlight�����	#75FFFFFF$y�	Collapsed=��� # z # z��$��2��#	 z	 #��z�� P�  �@J�IsMouseOver . . ���  � �� �� $	 0,1&�$	 0,0&�.��� $ 	#2FF1FBFF��$ 0Q�� $ 	#2FD5F1FE��$ 1Q���  � �� �	#5FCCF0FF�� $ UpperHighlight�� � y�$ Visible=�P�  �@��
IsSelected . . ���  � �� �� $	 0,1&�$	 0,0&�.��� $ 	#5FD9F4FF��$ 0Q�� $ 	#5F9BDDFB��$ 1Q���  � �� �	#3F98DDFB�� $ InnerBorder�� � �� �	#80FFFFFF�� $ UpperHighlight�� � y�$ Visible=��� $ UpperHighlight�� � �� �	#40FFFFFF]�  ��  �@��
IsSelected . ��  �@��IsSelectionActive .  ./���  � �� �� $	 0,1&�$	 0,0&�.��� $ 	#5FEEEDED��$ 0Q�� $ 	#4FDDDDDD��$ 1Q���  � �� �	#7FCFCFCF]�  ��  �@��
IsSelected . ��  �@J�IsMouseOver . ./���  � �� �� $	 0,1&�$	 0,0&�.��� $ 	#6FEAF9FF��$ 0Q�� $ 	#5FC9EDFD��$ 1Q���  � �� �	#3F98DDFBP�  � {� .  . ��� # � �� � ������  j� Stretch$
 Fill=� �� ImageSource$( "Resources/LoginPage_Background.png��.���  �� $
��auto ��� $
��auto ��� .+��� -   $��SelectedGameName��$��0��$��Stretch=�$��23��$��
110,10,0,0q� �� TextWrapping$
 Wrap=�$��Game by PlayerName��$��Center=�
���������Z� �� $��0��$
��Auto��$��	Game name��$
��Left=�$��Center=�
���������  �� Text$ Public��$��1��$
��Left=�$	��Top=�$	��100�� �� 
IsEditable .   �� 
IsReadOnly  . �� $ Genre��$��1��$
��Left=�$	��Top=�$	��120��$��
120,-1,0,0q� .   . �� $��1��$
��Auto��$��Filter name:��$
��Left=�$��Center=�$��	250,0,0,0q�
��������� $��1��$��Stretch=�$��	350,0,0,0q�$	��180��$��23��$
 Wrap=�$�� ��$��Center=�
���������Z� � $��2��! � ColumnDefinitions! �� $
��495* ��� $
��Auto �.+��� -   $��MapListView��$��0��$��Stretch=�$��Stretch=�$��2��
���������Z� " �� View" ��    GridViewColumnHeaderStyle1!# �� ColumnHeaderContainerStyle## [  .P��� $��Genre��$ �� Width$
$ Auto��% �� DisplayMemberBinding% �� *MapCategory+�� $
��Name��$
$ Auto��% �� *	MapName+�� $��FileName��$
$ Auto��% �� *MapFileName+�� $
��Code��$$ 0��% �� *	MapHost+�� $��crc32��$$ 0��% �� *crc32+.@��� $��ListViewItem��$��0q��� -   $��	0,0,25,24q�$��Bottom=�$��1��$��Create game��$��85��$��27�����	#FF1F1F1F
�������